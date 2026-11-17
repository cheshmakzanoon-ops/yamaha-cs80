"""First-attempt exit gate for trusted exact CTest 3.31.6 dashboard Test.xml.

Kitware v3.31.6 cmCTestTestHandler.cxx emits Exit Code/Exit Value whenever status
is not COMPLETED OR ReturnValue is nonzero. A passed/Completed record without
those fields therefore implies raw exit0 for this frozen producer. This is not
a generic JUnit inference and must never be applied to another CTest version.
"""
import math,shlex,xml.etree.ElementTree as ET
from identity import require


def require_exit_zero(text, *, name, expected_argv):
    root=ET.fromstring(text)
    require(root.tag=='Site' and root.get('Generator')=='ctest-3.31.6','exact dashboard producer required')
    testing=root.findall('Testing');require(len(testing)==1,'one dashboard Testing required')
    tests=testing[0].findall('Test');require(len(tests)==1,'one dashboard test required')
    case=tests[0]
    require(case.get('Status')=='passed' and case.findtext('Name')==name,'dashboard case failed/not run/wrong name')
    require(all(len(case.findall(tag))==1 for tag in ('Name','FullName','FullCommandLine')),
            'duplicate/missing dashboard descriptor')
    full=case.findtext('FullName')
    listed=testing[0].findall('TestList/Test')
    require(len(listed)==1 and listed[0].text==full and bool(full),'dashboard inventory mismatch')
    require(shlex.split(case.findtext('FullCommandLine',''))==expected_argv,'dashboard executed argv differs')
    results=case.findall('Results');require(len(results)==1,'one results block required')
    measures={}
    for node in results[0].findall('NamedMeasurement'):
        key=node.get('name');values=node.findall('Value')
        require(key and key not in measures and len(values)==1,'duplicate/malformed dashboard measurement')
        measures[key]=values[0].text or ''
    require(measures.get('Completion Status')=='Completed','incomplete dashboard case')
    require('Exit Code' not in measures and 'Exit Value' not in measures,
            'nonzero/abnormal or ambiguous first-attempt exit')
    require(shlex.split(measures.get('Command Line',''))==expected_argv,'dashboard command measurement differs')
    try:duration=float(measures['Execution Time'])
    except (KeyError,ValueError):raise ValueError('missing execution time')
    require(math.isfinite(duration) and duration>=0,'invalid execution time')
    return {'case':name,'raw_exit':0,'inferred_from_frozen_ctest_format':True,
            'same_attempt_provenance_required':True,'production_qualified':False}
