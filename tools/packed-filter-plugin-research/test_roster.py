import copy,unittest
from roster import roster,same_roster,complete_junit
class RosterTests(unittest.TestCase):
    def setUp(self):
        self.f={'tests':[{'name':'one','command':['/build/test','--full'],'properties':[{'name':'TIMEOUT','value':120}]}]}
        self.x='<testsuite tests="1" failures="0"><testcase name="one" time="0.1"/></testsuite>'
    def test_complete(self):self.assertEqual(complete_junit(self.x,self.f)['complete_test_count'],1)
    def test_same(self):same_roster(self.f,copy.deepcopy(self.f))
    def test_command_changed(self):
        f=copy.deepcopy(self.f);f['tests'][0]['command'][1]='--compat-only'
        with self.assertRaises(ValueError):same_roster(self.f,f)
    def test_timeout_changed(self):
        f=copy.deepcopy(self.f);f['tests'][0]['properties'][0]['value']=999
        with self.assertRaises(ValueError):same_roster(self.f,f)
    def test_unresolved(self):
        del self.f['tests'][0]['command']
        with self.assertRaises(ValueError):roster(self.f)
    def test_disabled(self):
        for v in (True,'ON','yes',1,'TRUE'):
            with self.subTest(v=v):
                self.f['tests'][0]['properties']=[{'name':'DISABLED','value':v}]
                with self.assertRaises(ValueError):roster(self.f)
    def test_false_disabled(self):
        for v in (False,'OFF','0',0):
            self.f['tests'][0]['properties']=[{'name':'DISABLED','value':v}];roster(self.f)
    def test_omission(self):
        with self.assertRaises(ValueError):complete_junit('<testsuite/>',self.f)
    def test_duplicate(self):
        with self.assertRaises(ValueError):complete_junit('<testsuite><testcase name="one" time="1"/><testcase name="one" time="1"/></testsuite>',self.f)
    def test_failure_statuses(self):
        for tag in ('failure','error','skipped'):
            with self.subTest(tag=tag),self.assertRaises(ValueError):complete_junit(f'<testsuite><testcase name="one" time="1"><{tag}/></testcase></testsuite>',self.f)
    def test_suite_failure(self):
        with self.assertRaises(ValueError):complete_junit(self.x.replace('failures="0"','failures="1"'),self.f)
    def test_container_failure(self):
        with self.assertRaises(ValueError):complete_junit('<testsuites failures="1">'+self.x+'</testsuites>',self.f)
    def test_suite_test_count(self):
        with self.assertRaises(ValueError):complete_junit(self.x.replace('tests="1"','tests="2"'),self.f)
    def test_container_test_count(self):
        with self.assertRaises(ValueError):complete_junit('<testsuites tests="2">'+self.x+'</testsuites>',self.f)
    def test_good_container(self):
        complete_junit('<testsuites tests="1" errors="0">'+self.x+'</testsuites>',self.f)
    def test_nonfinite_time(self):
        with self.assertRaises(ValueError):complete_junit(self.x.replace('0.1','nan'),self.f)
    def test_notrun(self):
        with self.assertRaises(ValueError):complete_junit(self.x.replace('time=', 'status="notrun" time='),self.f)
    def test_duplicate_property(self):
        self.f['tests'][0]['properties']*=2
        with self.assertRaises(ValueError):roster(self.f)
if __name__=='__main__':unittest.main()
