
import collections,io,json,pathlib,re,sys,unittest
root,report=map(pathlib.Path,sys.argv[1:])
suite=unittest.defaultTestLoader.discover(str(root/'tests/release'),pattern='test_distribution_source_identity.py')
result=unittest.TextTestRunner(verbosity=2).run(suite)
families=collections.Counter(re.search(r'\.(test_\w+)(?:\s|$)',test.id())[1] for test,_ in result.failures)
report.write_text(json.dumps({'tests':result.testsRun,'failures':[test.id() for test,_ in result.failures],
 'failure_families':dict(families),'errors':[test.id() for test,_ in result.errors],
 'skipped':[str(test) for test,_ in result.skipped],'unexpected_successes':len(result.unexpectedSuccesses),
 'expected_failures':len(result.expectedFailures)},indent=2)+'\n')
sys.exit(0 if result.wasSuccessful() else 1)
