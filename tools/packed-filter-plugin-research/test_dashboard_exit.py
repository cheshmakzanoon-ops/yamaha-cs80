import unittest
from dashboard_exit import require_exit_zero
BASE='''<Site Generator="ctest-3.31.6"><Testing><TestList><Test>./one</Test></TestList><Test Status="passed"><Name>one</Name><FullName>./one</FullName><FullCommandLine>/test arg</FullCommandLine><Results><NamedMeasurement name="Completion Status"><Value>Completed</Value></NamedMeasurement><NamedMeasurement name="Command Line"><Value>/test arg</Value></NamedMeasurement><NamedMeasurement name="Execution Time"><Value>0.1</Value></NamedMeasurement></Results></Test></Testing></Site>'''
class DashboardExitTests(unittest.TestCase):
    def call(self,s=BASE):return require_exit_zero(s,name='one',expected_argv=['/test','arg'])
    def test_exact_zero(self):self.assertEqual(self.call()['raw_exit'],0)
    def test_regex_pass_nonzero(self):
        extra='<NamedMeasurement name="Exit Code"><Value>Completed</Value></NamedMeasurement><NamedMeasurement name="Exit Value"><Value>42</Value></NamedMeasurement>'
        with self.assertRaises(ValueError):self.call(BASE.replace('</Results>',extra+'</Results>'))
    def test_ambiguous_explicit_zero(self):
        with self.assertRaises(ValueError):self.call(BASE.replace('</Results>','<NamedMeasurement name="Exit Value"><Value>0</Value></NamedMeasurement></Results>'))
    def test_wrong_producer(self):
        with self.assertRaises(ValueError):self.call(BASE.replace('3.31.6','3.31.10'))
    def test_failure_notrun(self):
        for status in ('failed','notrun'):
            with self.subTest(status=status),self.assertRaises(ValueError):self.call(BASE.replace('Status="passed"','Status="'+status+'"'))
    def test_missing_completion(self):
        with self.assertRaises(ValueError):self.call(BASE.replace('Completed','Other'))
    def test_argv_drift(self):
        with self.assertRaises(ValueError):self.call(BASE.replace('/test arg','/other arg'))
    def test_duplicate_name(self):
        with self.assertRaises(ValueError):self.call(BASE.replace('<Name>one</Name>','<Name>one</Name><Name>other</Name>'))
    def test_duplicate_measurement(self):
        with self.assertRaises(ValueError):self.call(BASE.replace('</Results>','<NamedMeasurement name="Execution Time"><Value>1</Value></NamedMeasurement></Results>'))
    def test_nonfinite_duration(self):
        with self.assertRaises(ValueError):self.call(BASE.replace('0.1','nan'))
    def test_testlist_mismatch(self):
        with self.assertRaises(ValueError):self.call(BASE.replace('<TestList><Test>./one','<TestList><Test>./other'))
if __name__=='__main__':unittest.main()
