#!/usr/bin/env python3
"""Complete two known collection cases without executing upstream source.
NIS puts its full MIT grant in README.md; Gamescope pins three freedesktop GitLab submodules.
Existing source bytes are verified before any additive repair.
"""
from __future__ import annotations
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile
import urllib.parse
import urllib.request
import collect_sources as c

ROOT = c.ROOT

def verify_existing(record):
    d = ROOT / record['destination']
    catalog = ROOT / 'provenance' / record['repo'].replace('/', '__')
    for f in json.loads((catalog / 'FILES.json').read_text(encoding='utf-8')):
        p = d.joinpath(*c.relative(f['path']).parts)
        if not p.is_file() or c.sha256(p) != f['sha256']:
            raise RuntimeError('Existing source differs from saved checksum: ' + f['path'])
    return d

def seal(record, tree):
    files = [p for p in sorted(tree.rglob('*')) if p.is_file() and not p.is_symlink()]
    index = [{'path': p.relative_to(tree).as_posix(), 'bytes': p.stat().st_size, 'sha256': c.sha256(p)} for p in files]
    record['file_count'] = len(files)
    record['source_bytes'] = sum(f['bytes'] for f in index)
    partial = bool(record.get('lfs_pointers')) or any(s['status'] == 'missing' for s in record['submodules'])
    partial |= any('manual archival' in f['reason'] for f in record.get('omitted_files', []))
    record['status'] = 'partial' if partial else 'collected'
    record['compiled'] = False
    record['runtime_tested'] = False
    catalog = ROOT / 'provenance' / record['repo'].replace('/', '__')
    catalog.mkdir(parents=True, exist_ok=True)
    (catalog / 'FILES.json').write_text(json.dumps(index, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    (catalog / 'ORIGIN.json').write_text(json.dumps(record, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    return record

def collect_nis(previous):
    repo = 'NVIDIAGameWorks/NVIDIAImageScaling'
    if previous.get('status') == 'collected':
        verify_existing(previous)
        return previous
    meta = c.api(repo, '')
    ref = meta['default_branch']
    commit = c.api(repo, 'commits/' + urllib.parse.quote(ref, safe=''))['sha']
    dest = ROOT / 'third_party' / repo.replace('/', '__') / commit
    record = {'repo': repo, 'source_url': 'https://github.com/' + repo, 'ref': ref,
              'commit': commit, 'purpose': 'NIS spatial upscaling; NVScaler includes sharpening',
              'declared_license': 'MIT', 'github_license': (meta.get('license') or {}).get('spdx_id'),
              'license_files': ['README.md'], 'license_note': 'Full MIT license is embedded at the start of upstream README.md; no invented LICENSE file',
              'destination': dest.relative_to(ROOT).as_posix(), 'submodules': []}
    with tempfile.TemporaryDirectory() as td:
        tree = Path(td) / 'tree'
        c.download_tree(repo, commit, tree, record['submodules'])
        grant = (tree / 'README.md').read_text(encoding='utf-8').lower()
        required = ['the mit license', 'permission is hereby granted', 'copyright', 'permission notice shall be included']
        if not all(text in grant for text in required):
            raise RuntimeError('NIS MIT grant no longer matches the reviewed README; stop')
        record['omitted_files'], record['lfs_pointers'], record['symlinks'] = c.sanitize_tree(tree, [])
        if dest.exists():
            raise RuntimeError('Unexpected NIS destination exists; preserving it')
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.move(str(tree), str(dest))
        return seal(record, dest)

GITLAB_PROJECTS = {
    'subprojects/wlroots': 'wlroots/wlroots',
    'subprojects/libliftoff': 'emersion/libliftoff',
    'subprojects/libdisplay-info': 'emersion/libdisplay-info',
}

def complete_gamescope(record):
    root = verify_existing(record)
    for module in record['submodules']:
        if module.get('status') != 'missing' or module['path'] not in GITLAB_PROJECTS:
            continue
        project = GITLAB_PROJECTS[module['path']]
        try:
            canonical = 'https://gitlab.freedesktop.org/' + project
            if module['url'].rstrip('/').removesuffix('.git') != canonical:
                raise RuntimeError('Unexpected external submodule URL')
            info = c.api(record['repo'], 'contents/' + module['path'] + '?ref=' + record['commit'])
            sha = info['sha']
            if not c.SHA_RE.fullmatch(sha) or not info.get('submodule_git_url'):
                raise RuntimeError('Invalid pinned submodule metadata')
            module['commit'] = sha
            archive_url = f'{canonical}/-/archive/{sha}/{project.split("/")[-1]}-{sha}.tar.gz'
            # No GitHub token or other credentials are sent to GitLab.
            with tempfile.TemporaryDirectory() as td:
                td = Path(td)
                req = urllib.request.Request(archive_url, headers={'User-Agent': 'Game-HD2D-Enhancer-source-collector'})
                with c.OPENER.open(req, timeout=90) as response, (td / 'source.tar.gz').open('wb') as f:
                    total = 0
                    while chunk := response.read(1024 * 1024):
                        total += len(chunk)
                        if total > c.MAX_BYTES:
                            raise RuntimeError('External source archive too large')
                        f.write(chunk)
                tree = c.unpack(td / 'source.tar.gz', td / 'unpacked')
                c.license_evidence(tree, 'check-upstream')
                if (tree / '.gitmodules').exists():
                    raise RuntimeError('Nested external submodules require a separate review')
                omitted, lfs, links = c.sanitize_tree(tree, [])
                prefix = module['path'] + '/'
                for x in omitted:
                    x['path'] = prefix + x['path']
                for x in links:
                    x['path'] = prefix + x['path']
                dest = root.joinpath(*c.relative(module['path']).parts)
                if dest.exists():
                    if any(dest.iterdir()):
                        raise RuntimeError('Refusing to overwrite existing dependency source')
                    dest.rmdir()
                dest.parent.mkdir(parents=True, exist_ok=True)
                shutil.move(str(tree), str(dest))
                record['omitted_files'].extend(omitted)
                record['lfs_pointers'].extend(prefix + x for x in lfs)
                record['symlinks'].extend(links)
            module.update(status='downloaded', archive_url=archive_url)
            module.pop('error', None)
        except Exception as exc:
            module.update(status='missing', error=str(exc)[:500])
        print(json.dumps(module), flush=True)
    return seal(record, root)

def main():
    lock = ROOT / 'sources.lock.json'
    data = json.loads(lock.read_text(encoding='utf-8'))
    for i, record in enumerate(data['sources']):
        try:
            if record['repo'] == 'NVIDIAGameWorks/NVIDIAImageScaling':
                data['sources'][i] = collect_nis(record)
            elif record['repo'] == 'ValveSoftware/gamescope' and record.get('status') == 'partial':
                data['sources'][i] = complete_gamescope(record)
        except Exception as exc:
            record['completion_error'] = str(exc)[:700]
            print(record['repo'] + ': ' + str(exc), flush=True)
    data['updated_utc'] = datetime.now(timezone.utc).isoformat()
    lock.write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    count = sum(r.get('file_count', 0) for r in data['sources'])
    size = sum(r.get('source_bytes', 0) for r in data['sources'])
    lines = ['# Game-HD2D-Enhancer 源码收集结果', '', '更新时间：' + data['updated_utc'], '',
             f'实际保存 {count:,} 个上游文件，共 {size:,} 字节（含源码、构建文件、许可证、必要资源及文档）。', '',
             '**这是开发源码资料库，不是已编译或实机验证的 Game-HD2D-Enhancer 软件。**', '',
             'collected：源码快照已入库；partial：仍有明确缺失项；not_collected：未保存该项目。',
             '预编译程序/库、模型权重、演示视频等不是本次源码交付，逐项排除记录在 provenance/*/ORIGIN.json。',
             '符号链接目标记录在 ORIGIN.json；构建前按需恢复。不可声称所有 SDK/二进制依赖都已齐备。',
             '所有上游代码仅作为文件保存，没有运行。每个文件都有SHA-256；上游提交和子模块提交已记录。', '',
             '| 项目 | 状态 | 文件数 | 固定提交 |', '|---|---|---:|---|']
    for r in data['sources']:
        lines.append(f"| {r['repo']} | {r['status']} | {r.get('file_count', 0)} | {r.get('commit', '-')[:12]} |")
    lines += ['', '## 明确缺失项', '']
    missing = []
    for r in data['sources']:
        if r.get('error') or r.get('completion_error'):
            missing.append(r['repo'] + ': ' + r.get('completion_error', r.get('error', '')))
        for s in r.get('submodules', []):
            if s['status'] == 'missing':
                missing.append(r['repo'] + ' / ' + s['path'] + ': ' + s.get('error', 'not downloaded'))
        missing += [r['repo'] + ' LFS: ' + p for p in r.get('lfs_pointers', [])]
    lines += ['- ' + m.replace('\n', ' ') for m in missing] or ['无未说明的源代码下载缺失；仍需单独完成构建和实际运行测试。']
    lines += ['', '## 按要求或许可排除', ''] + ['- ' + r['repo'] + '：' + r['reason'] for r in data['excluded']]
    lines += ['', '## 2D版本下一步', '', '先完成WGC/D3D11独立启停闭环，再接入整数缩放、NIS/FSR1和输入坐标映射。',
              '相关Linux/注入/插帧项目只作资料存档，不参与Windows 2D V1。DLSS5不纳入。', '']
    report = '\n'.join(lines)
    (ROOT / 'SOURCE_COLLECTION_STATUS.md').write_text(report, encoding='utf-8')
    if os.environ.get('GITHUB_STEP_SUMMARY'):
        with open(os.environ['GITHUB_STEP_SUMMARY'], 'a', encoding='utf-8') as f:
            f.write(report)
    return int(any(r['status'] != 'collected' for r in data['sources']))

if __name__ == '__main__':
    sys.exit(main())
