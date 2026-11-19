from copy import deepcopy
import unittest
from replay import load,replay

class Evidence(unittest.TestCase):
    def test_whole_Q(self):
        self.assertEqual(replay(load())['verified_gates'],35)
    def test_negative(self):
        for alter in (lambda d:d['slots'].pop(),lambda d:d['cells'].reverse(),
                      lambda d:d['cells'][0]['real_thread_ns'].update(B_1_0=10),
                      lambda d:d['cells'][0].update(empty_ns_sum=10**15),
                      lambda d:d['cells'][0].update(q1_gap_ns_twice_median=10**15),
                      lambda d:d['terminal'].update(candidate_stage_authorized=True),
                      lambda d:d['original_decision']['gates'].update({'idle:twins-equivalence':False}),
                      lambda d:d['slots'][0].update(end_ns=10**15)):
            d=deepcopy(load());alter(d)
            with self.assertRaises(ValueError):replay(d)
if __name__=='__main__':unittest.main()
