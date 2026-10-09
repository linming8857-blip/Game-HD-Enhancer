#!/usr/bin/env python3
"""Archive source trees only; never execute, build, or import upstream code.
Python 3.12+, standard library only. GitHub API credentials are sent only to api.github.com.
"""
from __future__ import annotations
import configparser
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import sys
import tarfile
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[1]
REPO_RE = re.compile(r"^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$")
SHA_RE = re.compile(r"^[0-9a-f]{40}$")
# These are distributed/generated binaries or demo media, NOT source files.
OMIT_SUFFIX = {'.exe', '.dll', '.lib', '.pdb', '.obj', '.o', '.a', '.so', '.dylib',
               '.pyd', '.pyc', '.zip', '.7z', '.rar', '.mp4', '.webm', '.mkv',
               '.pth', '.onnx', '.cso', '.spv'}
MAX_BYTES = 2 * 1024**3

class SafeRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        old, new = urllib.parse.urlparse(req.full_url), urllib.parse.urlparse(newurl)
        if new.scheme != 'https' or new.hostname != old.hostname:
            raise RuntimeError('Cross-host redirect refused')
        return super().redirect_request(req, fp, code, msg, headers, newurl)

OPENER = urllib.request.build_opener(SafeRedirect())

def request(url: str, output: Path | None = None):
    host = urllib.parse.urlparse(url).hostname
    if host not in {'api.github.com', 'codeload.github.com'}:
        raise ValueError('Only public GitHub API/codeload endpoints are allowed')
    headers = {'User-Agent': 'Game-HD2D-Enhancer-source-collector'}
    if host == 'api.github.com':
        headers['Accept'] = 'application/vnd.github+json'
        token = os.environ.get('GH_TOKEN')
        if token:
            headers['Authorization'] = 'Bearer ' + token
    for attempt in range(3):
        try:
            with OPENER.open(urllib.request.Request(url, headers=headers), timeout=120) as response:
                if output is None:
                    return json.load(response)
                total = 0
                with output.open('wb') as dest:
                    while chunk := response.read(1024 * 1024):
                        total += len(chunk)
                        if total > MAX_BYTES:
                            raise RuntimeError('Archive exceeded safety size limit')
                        dest.write(chunk)
                return None
        except (urllib.error.URLError, TimeoutError):
            if attempt == 2:
                raise
            time.sleep(2 ** attempt)

def api(repo: str, path: str):
    if not REPO_RE.fullmatch(repo):
        raise ValueError('Invalid repository name')
    return request(f'https://api.github.com/repos/{repo}' + ('/' + path if path else ''))

def relative(path: str) -> PurePosixPath:
    p = PurePosixPath(path)
    if p.is_absolute() or '..' in p.parts or not p.parts or '\\' in path:
        raise ValueError(f'Unsafe path: {path!r}')
    return p

def unpack(archive: Path, destination: Path):
    destination.mkdir(parents=True, exist_ok=True)
    with tarfile.open(archive, 'r:gz') as tf:
        members = tf.getmembers()
        if sum(m.size for m in members if m.isfile()) > MAX_BYTES:
            raise RuntimeError('Expanded archive exceeded safety size limit')
        for m in members:
            relative(m.name)
            if not (m.isfile() or m.isdir() or m.issym() or m.islnk()):
                raise ValueError('Special archive member refused')
        tf.extractall(destination, filter='data')
    roots = list(destination.iterdir())
    if len(roots) != 1 or not roots[0].is_dir():
        raise ValueError('Unexpected codeload root layout')
    return roots[0]

def normalize_submodule(parent_repo: str, url: str) -> str:
    if url.startswith(('./', '../')):
        url = urllib.parse.urljoin(f'https://github.com/{parent_repo}/', url)
    if url.startswith('git@github.com:'):
        url = 'https://github.com/' + url.split(':', 1)[1]
    u = urllib.parse.urlparse(url)
    if u.scheme != 'https' or u.hostname != 'github.com' or u.username or u.query:
        raise ValueError('Submodule is not a public github.com HTTPS source')
    repo = u.path.strip('/').removesuffix('.git')
    if not REPO_RE.fullmatch(repo):
        raise ValueError('Invalid submodule repository')
    return repo

def download_tree(repo: str, sha: str, dest: Path, records: list, prefix='', depth=0):
    if not SHA_RE.fullmatch(sha) or depth > 6:
        raise ValueError('Invalid commit SHA or excessive submodule depth')
    with tempfile.TemporaryDirectory() as td:
        td = Path(td)
        request(f'https://codeload.github.com/{repo}/tar.gz/{sha}', td / 'source.tar.gz')
        source = unpack(td / 'source.tar.gz', td / 'unpacked')
        # Move data only. No Git hooks, package managers, or upstream scripts are run.
        if dest.exists():
            if any(dest.iterdir()):
                raise RuntimeError('Refusing to overwrite a nonempty source directory')
            dest.rmdir()
        shutil.move(str(source), str(dest))
    gm = dest / '.gitmodules'
    if not gm.is_file():
        return
    parser = configparser.RawConfigParser()
    parser.read(gm, encoding='utf-8')
    for section in parser.sections():
        path = parser.get(section, 'path').strip('"')
        url = parser.get(section, 'url').strip('"')
        rel = relative(path)
        fullpath = prefix + path
        item = {'path': fullpath, 'url': url}
        try:
            subrepo = normalize_submodule(repo, url)
            info = api(repo, 'contents/' + urllib.parse.quote(path, safe='/') + '?ref=' + sha)
            subsha = info['sha']
            if info.get('type') not in {'submodule', 'file'} or not info.get('submodule_git_url'):
                raise ValueError('Path is not a Git submodule')
            item.update(repository=subrepo, commit=subsha)
            download_tree(subrepo, subsha, dest.joinpath(*rel.parts), records, fullpath + '/', depth + 1)
            item['status'] = 'downloaded'
        except Exception as exc:
            item.update(status='missing', error=str(exc)[:400])
        records.append(item)

def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()

def sanitize_tree(tree: Path, skip_roots: list[str]):
    omitted, lfs, links = [], [], []
    for p in sorted(tree.rglob('*')):
        rel = p.relative_to(tree).as_posix()
        if p.is_symlink():
            # Preserve original link semantics as metadata rather than following it into the host.
            links.append({'path': rel, 'target': os.readlink(p)})
            p.unlink()
            continue
        if not p.is_file():
            continue
        reason = None
        if any(rel == r or rel.startswith(r.rstrip('/') + '/') for r in skip_roots):
            reason = 'upstream demo assets explicitly excluded; not source code'
        elif p.suffix.lower() in OMIT_SUFFIX:
            reason = 'prebuilt binary, packaged release, model weight, or demo video'
        elif '.git' in p.relative_to(tree).parts:
            reason = 'Git internal metadata'
        elif p.stat().st_size > 90 * 1024**2:
            reason = 'exceeds safe Git blob limit; manual archival required'
        else:
            with p.open('rb') as f:
                header = f.read(160)
            if header.startswith(b'version https://git-lfs.github.com/spec/v1'):
                lfs.append(rel)
        if reason:
            omitted.append({'path': rel, 'bytes': p.stat().st_size, 'reason': reason})
            p.unlink()
    return omitted, lfs, links

def license_evidence(tree: Path, declared: str) -> list[str]:
    files = [p for p in tree.iterdir() if p.is_file() and
             p.name.lower().startswith(('license', 'copying', 'copyright'))]
    if declared == 'per-file':
        # This is an archival collection only; each effect still needs a reuse audit.
        return [p.relative_to(tree).as_posix() for p in files] or ['Per-file shader notices; no blanket project relicense']
    if not files:
        raise RuntimeError('No top-level license file found: not republishing this source')
    combined = '\n'.join(p.read_text(encoding='utf-8', errors='replace') for p in files).lower()
    grants = ('permission is hereby granted', 'redistribution and use', 'general public license',
              'apache license', 'permission to use', 'boost software license', 'public domain')
    if not any(g in combined for g in grants):
        raise RuntimeError('License grant could not be confirmed; source held for manual review')
    return [p.name for p in files]

def collect(entry: dict, locked: dict) -> dict:
    repo = entry['repo']
    meta = api(repo, '')
    branch = entry.get('ref') or meta['default_branch']
    sha = locked.get(repo) or api(repo, 'commits/' + urllib.parse.quote(branch, safe=''))['sha']
    slug = repo.replace('/', '__')
    destination = ROOT / entry['group'] / slug / sha
    report = {'repo': repo, 'source_url': 'https://github.com/' + repo, 'ref': branch,
              'commit': sha, 'purpose': entry['purpose'], 'declared_license': entry['license'],
              'github_license': (meta.get('license') or {}).get('spdx_id'),
              'destination': destination.relative_to(ROOT).as_posix(),
              'submodules': [], 'compiled': False, 'runtime_tested': False}
    if destination.exists():
        prior_dir = ROOT / 'provenance' / slug
        saved = json.loads((prior_dir / 'ORIGIN.json').read_text(encoding='utf-8'))
        manifest = json.loads((prior_dir / 'FILES.json').read_text(encoding='utf-8'))
        if saved.get('commit') != sha:
            raise RuntimeError('Existing snapshot origin does not match requested commit')
        for item in manifest:
            p = destination.joinpath(*relative(item['path']).parts)
            if not p.is_file() or sha256(p) != item['sha256']:
                raise RuntimeError(f'Existing source changed; preserving it: {item["path"]}')
        return saved
    with tempfile.TemporaryDirectory() as td:
        tree = Path(td) / 'tree'
        download_tree(repo, sha, tree, report['submodules'])
        report['license_files'] = license_evidence(tree, entry['license'])
        report['omitted_files'], report['lfs_pointers'], report['symlinks'] = sanitize_tree(tree, entry.get('skip_roots', []))
        files = [p for p in sorted(tree.rglob('*')) if p.is_file()]
        manifest = [{'path': p.relative_to(tree).as_posix(), 'bytes': p.stat().st_size,
                     'sha256': sha256(p)} for p in files]
        report.update(file_count=len(files), source_bytes=sum(x['bytes'] for x in manifest))
        incomplete = bool(report['lfs_pointers']) or any(x['status'] == 'missing' for x in report['submodules'])
        incomplete |= any('manual archival' in x['reason'] for x in report['omitted_files'])
        report['status'] = 'partial' if incomplete else 'collected'
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.move(str(tree), str(destination))
        catalog_dir = ROOT / 'provenance' / slug
        catalog_dir.mkdir(parents=True, exist_ok=True)
        (catalog_dir / 'FILES.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
        (catalog_dir / 'ORIGIN.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    return report

def main() -> int:
    spec = json.loads((ROOT / 'sources.json').read_text(encoding='utf-8'))
    old = ROOT / 'sources.lock.json'
    previous = json.loads(old.read_text(encoding='utf-8')) if old.exists() else {'sources': []}
    locked = {x['repo']: x['commit'] for x in previous['sources'] if x.get('commit')}
    results = []
    for entry in spec['sources']:
        print('Collecting ' + entry['repo'], flush=True)
        try:
            result = collect(entry, locked)
        except Exception as exc:
            result = {'repo': entry['repo'], 'status': 'not_collected', 'error': str(exc)[:700]}
        results.append(result)
        print(json.dumps({k: result[k] for k in ('repo', 'status', 'file_count', 'source_bytes', 'error') if k in result}), flush=True)
    data = {'project': 'Game-HD2D-Enhancer', 'created_utc': datetime.now(timezone.utc).isoformat(),
            'scope': 'Actual source snapshots, not executables. DLSS5 excluded. Original licenses retained.',
            'sources': results, 'excluded': spec['excluded']}
    old.write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    count = sum(x.get('file_count', 0) for x in results)
    size = sum(x.get('source_bytes', 0) for x in results)
    lines = ['# Game-HD2D-Enhancer 源码收集结果', '',
             f"生成时间：{data['created_utc']}", '',
             f'实际保存 {count:,} 个原始文件，共 {size:,} 字节。', '',
             '**这是开发源码资料库，不是已编译或实机验证的高清软件。**', '',
             '源码与原始许可证一同保存；不自动执行第三方安装脚本、不运行第三方二进制。',
             'collected 表示源码快照已入库，不代表运行依赖/SDK/模型/演示资源全部齐备。',
             '预编译二进制、模型权重、演示视频等排除项逐一记录于 provenance/*/ORIGIN.json。',
             'partial 表示仍有缺少的子模块、LFS对象或超大文件；不得称为齐全。', '',
             '| 项目 | 状态 | 文件数 | 固定提交 |', '|---|---|---:|---|']
    for r in results:
        lines.append(f"| {r['repo']} | {r['status']} | {r.get('file_count', 0)} | {r.get('commit', '-')[:12]} |")
        if r.get('error'):
            lines.append(f"\n> {r['repo']}: {r['error'].replace(chr(10), ' ')}\n")
    lines += ['', '## 未纳入', ''] + [f"- {r['repo']}：{r['reason']}" for r in spec['excluded']]
    lines += ['', '## 下一步', '',
              '先完成独立 WGC/D3D11 捕获与关闭恢复，再接入整数缩放和轻量空间放大。',
              '鼠标缩放坐标映射、焦点、Alt-Tab、窗口最小化、帧队列和掌机功耗必须单独测试。',
              '仅有点击穿透不代表缩放后点击坐标必然正确；仅有着色器不代表已完成应用集成。', '']
    text = '\n'.join(lines)
    (ROOT / 'SOURCE_COLLECTION_STATUS.md').write_text(text, encoding='utf-8')
    if os.environ.get('GITHUB_STEP_SUMMARY'):
        with open(os.environ['GITHUB_STEP_SUMMARY'], 'a', encoding='utf-8') as f:
            f.write(text)
    return int(any(x['status'] != 'collected' for x in results))

if __name__ == '__main__':
    sys.exit(main())
