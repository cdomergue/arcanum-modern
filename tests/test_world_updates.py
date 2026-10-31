"""Package publication and native codec compatibility checks; all data is temporary."""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('world_package',ROOT/'tools/world-updates/package.py')
package=importlib.util.module_from_spec(spec);spec.loader.exec_module(package)

class Packages(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix='world-update-test-')
        self.root=Path(self.tmp.name);self.game=self.root/'game';(self.game/'modules').mkdir(parents=True)
        self.source=self.root/'source';self.source.mkdir()
        self.base=dict(format='renaissance-world-update',format_version=1,package_id='test.base',module='Arcanum',revision=1,version='1.0',requires=[],resources=[],operations=[dict(id='flag',revision=1,kind='global',field='global_flag',index=3100,before=['0','0','0'],after=['1','0','0'])])
    def tearDown(self):self.tmp.cleanup()
    def compiled(self,data,name):
        source=self.source/(name+'.json');source.write_text(json.dumps(data),encoding='utf-8')
        return package.compile_package(source,self.root/name)
    def test_install_replay_skipped_revision_and_immutability(self):
        first=self.compiled(self.base,'v1');target=package.install_package(first,self.game)
        registry=self.game/'ArcanumModern/world-updates/index.mes';original=registry.read_bytes()
        self.assertEqual(target,package.install_package(first,self.game));self.assertEqual(original,registry.read_bytes())
        new=copy.deepcopy(self.base);new['revision']=3;new['version']='1.2';new['operations'].append(dict(id='second',revision=3,kind='global',field='global_var',index=1800,before=['0','0','0'],after=['7','0','0']))
        third=self.compiled(new,'v3');package.install_package(third,self.game)
        self.assertTrue(target.exists());self.assertIn('test.base/3',registry.read_text())
        with self.assertRaisesRegex(ValueError,'downgrade'):package.install_package(first,self.game)
        bad=copy.deepcopy(new);bad['operations'][0]['after'][0]='0'
        with self.assertRaisesRegex(ValueError,'Published operation'):package.validate_upgrade(new,bad)
        bad=copy.deepcopy(new);bad['operations'].append(dict(id='retroactive',revision=1,kind='global',field='global_flag',index=3101))
        with self.assertRaisesRegex(ValueError,'published history'):package.validate_upgrade(new,bad)
    def test_dependencies_and_cycle_leave_registry_intact(self):
        first=self.compiled(self.base,'v1');package.install_package(first,self.game)
        registry=self.game/'ArcanumModern/world-updates/index.mes';original=registry.read_bytes()
        child=copy.deepcopy(self.base);child['package_id']='test.child';child['requires']=[dict(package_id='missing',min_revision=1)]
        with self.assertRaisesRegex(ValueError,'dependency'):package.install_package(self.compiled(child,'missing'),self.game)
        self.assertEqual(original,registry.read_bytes())
        child['requires']=[dict(package_id='test.base',min_revision=1)];package.install_package(self.compiled(child,'child'),self.game)
        upgrade=copy.deepcopy(self.base);upgrade['revision']=2;upgrade['requires']=[dict(package_id='test.child',min_revision=1)]
        with self.assertRaisesRegex(ValueError,'cycle'):package.install_package(self.compiled(upgrade,'cycle'),self.game)
    def test_interrupted_publication_keeps_prior_registry(self):
        first=self.compiled(self.base,'v1');package.install_package(first,self.game)
        registry=self.game/'ArcanumModern/world-updates/index.mes';original=registry.read_bytes()
        new=copy.deepcopy(self.base);new['revision']=2;second=self.compiled(new,'v2')
        replace=package.os.replace
        def failing(source,target):
            if Path(target)==registry:raise OSError('simulated publication failure')
            return replace(source,target)
        with patch.object(package.os,'replace',side_effect=failing):
            with self.assertRaises(OSError):package.install_package(second,self.game)
        self.assertEqual(original,registry.read_bytes());package.install_package(second,self.game)
        self.assertIn('test.base/2',registry.read_text())
    def test_unordered_writes_and_resource_aliases(self):
        first=copy.deepcopy(self.base);other=copy.deepcopy(first);other['package_id']='test.other'
        with self.assertRaisesRegex(ValueError,'Overlapping'):
            package.validate_composition({'test.base':first,'test.other':other})
        other['requires']=[dict(package_id='test.base',min_revision=1)]
        package.validate_composition({'test.base':first,'test.other':other})
        first['resources']=[dict(path='scr/00123_first.scr',sha256='0'*64)]
        other['resources']=[dict(path='scr/123_second.scr',sha256='0'*64)]
        with self.assertRaisesRegex(ValueError,'numeric IDs'):
            package.validate_composition({'test.base':first,'test.other':other})

    def test_bundled_registry_survives_manual_install(self):
        first=self.compiled(self.base,'v1');package.install_package(first,self.game)
        root=self.game/'ArcanumModern/world-updates'
        (root/'index.mes').rename(root/'bundled.mes');bundled=(root/'bundled.mes').read_bytes()
        child=copy.deepcopy(self.base);child['package_id']='test.child';child['requires']=[dict(package_id='test.base',min_revision=1)]
        package.install_package(self.compiled(child,'child'),self.game)
        self.assertEqual(bundled,(root/'bundled.mes').read_bytes())
        self.assertEqual(package.registry_read(root),{'test.child':'test.child/1'})
        upgrade=copy.deepcopy(self.base);upgrade['revision']=2
        package.install_package(self.compiled(upgrade,'v2'),self.game)
        self.assertEqual(bundled,(root/'bundled.mes').read_bytes())
        self.assertEqual(package.registry_read(root)['test.base'],'test.base/2')

    def test_corrupt_resources_and_path_escape(self):
        (self.source/'objects').mkdir();data=b'fixture';(self.source/'objects/test.mob').write_bytes(data)
        manifest=copy.deepcopy(self.base);manifest['resources']=[dict(path='objects/test.mob',sha256=hashlib.sha256(data).hexdigest())]
        folder=self.compiled(manifest,'resource');(folder/'objects/test.mob').write_bytes(b'corrupt')
        with self.assertRaisesRegex(ValueError,'digest'):package.load_package(folder)
        for value in ('../outside','/absolute','objects/../x','objects/X.mob','objects\\x'):
            with self.assertRaises(ValueError):package.relative(value)
    def test_worlded_binary_matches_compiler_and_native_reader(self):
        fixture=ROOT/'build/world-update-export-fixture';native=ROOT/'build/world-update-codec-tests/Release/world-update-codec-check.exe'
        if not fixture.exists() or not native.exists():self.skipTest('Build exporter fixture and native codec first')
        manifest,_=package.load_package(fixture)
        self.assertEqual(len(manifest['operations']),3)
        subprocess.run([str(native),str(fixture)],check=True,capture_output=True)
        # Malformed bounded strings and truncated files must be rejected before native mutation.
        folder=self.compiled(self.base,'binary');runtime=folder/'runtime.rwp';raw=runtime.read_bytes()
        for broken in (raw[:7],raw[:12]+b'\xff'*4+raw[16:],raw+b'junk'):
            runtime.write_bytes(broken)
            self.assertNotEqual(subprocess.run([str(native),str(folder)],capture_output=True).returncode,0)

if __name__=='__main__':unittest.main()
