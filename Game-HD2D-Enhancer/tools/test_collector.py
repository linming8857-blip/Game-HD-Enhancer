"""Tests only our collector. Never imports or executes upstream repositories."""
import importlib.util
import io
from pathlib import Path
import tarfile
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('collector', Path(__file__).with_name('collect_sources.py'))
c = importlib.util.module_from_spec(spec)
spec.loader.exec_module(c)

class CollectorTests(unittest.TestCase):
    def test_submodule_urls(self):
        self.assertEqual(c.normalize_submodule('owner/project', '../lib.git'), 'owner/lib')
        self.assertEqual(c.normalize_submodule('owner/project', 'git@github.com:org/lib.git'), 'org/lib')
        for url in ['file:///etc/passwd', 'https://evil.invalid/org/repo', 'https://github.com/org/repo?token=x']:
            with self.assertRaises(ValueError):
                c.normalize_submodule('owner/project', url)

    def test_paths(self):
        for path in ['../escape', '/absolute', 'a/../../escape', 'C:\\escape']:
            with self.assertRaises(ValueError):
                c.relative(path)
        self.assertEqual(str(c.relative('src/main.cpp')), 'src/main.cpp')

    def test_archive(self):
        with tempfile.TemporaryDirectory() as td:
            td = Path(td)
            archive = td / 'sample.tar.gz'
            with tarfile.open(archive, 'w:gz') as tf:
                item = tarfile.TarInfo('repo/main.cpp')
                item.size = 5
                tf.addfile(item, io.BytesIO(b'hello'))
            root = c.unpack(archive, td / 'out')
            self.assertEqual((root / 'main.cpp').read_bytes(), b'hello')
            with tarfile.open(archive, 'w:gz') as tf:
                item = tarfile.TarInfo('../escape')
                item.size = 3
                tf.addfile(item, io.BytesIO(b'bad'))
            with self.assertRaises(ValueError):
                c.unpack(archive, td / 'bad')

    def test_source_preservation(self):
        with tempfile.TemporaryDirectory() as td:
            tree = Path(td)
            (tree / 'main.cpp').write_text('int main() {}')
            (tree / 'shader.hlsl').write_text('// shader source')
            (tree / 'LICENSE').write_text('Permission is hereby granted, free of charge')
            (tree / 'untrusted.exe').write_bytes(b'fake executable; never run')
            (tree / 'missing-lfs.dat').write_bytes(b'version https://git-lfs.github.com/spec/v1\n')
            omitted, lfs, links = c.sanitize_tree(tree, [])
            self.assertTrue((tree / 'main.cpp').is_file())
            self.assertTrue((tree / 'shader.hlsl').is_file())
            self.assertTrue((tree / 'LICENSE').is_file())
            self.assertFalse((tree / 'untrusted.exe').exists())
            self.assertEqual(omitted[0]['path'], 'untrusted.exe')
            self.assertEqual(lfs, ['missing-lfs.dat'])
            self.assertEqual(links, [])
            self.assertEqual(c.license_evidence(tree, 'MIT'), ['LICENSE'])

if __name__ == '__main__':
    unittest.main()
