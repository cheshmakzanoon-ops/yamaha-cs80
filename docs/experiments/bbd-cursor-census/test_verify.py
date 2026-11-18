import copy,unittest
import verify as v
class CensusTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.counts=v.load(v.ROOT/'counts.json');cls.profile=v.load(v.ROOT/'profile.json');cls.provenance=v.load(v.ROOT/'provenance.json');cls.sources=v.load(v.ROOT/'stock-sources.json')
 def bad(self,edit):
  values=copy.deepcopy([self.counts,self.profile,self.provenance,self.sources]);edit(*values)
  with self.assertRaises((ValueError,KeyError)):v.check_records(*values)
 def test_good_package(self):v.check_package()
 def test_missing_scene(self):self.bad(lambda c,p,i,s:c['parity'].pop('pure-held'))
 def test_doubled_chain_credit(self):self.bad(lambda c,p,i,s:c['whole_four_scene_totals'].update(eligible_adjacent_repeats=1182720))
 def test_ordinal_association(self):self.bad(lambda c,p,i,s:c['common_counts_for_every_scene']['buckets']['all']['per_tap_hits'].__setitem__(1,49280))
 def test_not_actual_taps(self):self.bad(lambda c,p,i,s:c['common_counts_for_every_scene']['buckets']['steady'].update(taps=16384))
 def test_false_mismatch_pass(self):self.bad(lambda c,p,i,s:c['parity']['factory-held'].update(stock_observed_equal=False))
 def test_incomplete_trace(self):self.bad(lambda c,p,i,s:c['parity']['factory-held'].update(bytes_per_arm=1))
 def test_candidate_is_not_stock(self):self.bad(lambda c,p,i,s:c.update(candidate_DSP_runs=1))
 def test_wrong_archive(self):self.bad(lambda c,p,i,s:i.update(stock_archive_sha256='0'*64))
 def test_missing_source(self):self.bad(lambda c,p,i,s:s.pop('core/Engine.cpp'))
 def test_profile_denominator(self):self.bad(lambda c,p,i,s:p['rows'][0].update(total_samples=5000))
 def test_confidence_claim(self):self.bad(lambda c,p,i,s:p.update(confidence_bound=True))
 def test_speed_forecast(self):self.bad(lambda c,p,i,s:p.update(speed_forecast=True))
 def test_no_fuzzy_hook_replacement(self):
  with self.assertRaises(ValueError):v.observed_header(b'not the pinned header')
if __name__=='__main__':unittest.main()
