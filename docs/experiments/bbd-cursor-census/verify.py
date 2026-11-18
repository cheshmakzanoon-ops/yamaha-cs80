#!/usr/bin/env python3
"""Offline integrity, count/provenance relationships, and optional local inputs."""
import argparse,hashlib,json,sys
from pathlib import Path,PurePosixPath
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parent
SHA=lambda b:hashlib.sha256(b).hexdigest()
def need(x,message):
 if not x:raise ValueError(message)
def load(p):return json.loads(p.read_bytes())
def observed_header(raw):
 entry=b'    Stereo process(float inL, float inR, float speed, float depth, bool on, bool tremolo) noexcept\n    {\n'
 material=b'        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);\n'
 need(raw.count(entry)==raw.count(material)==1,'unique original hook sites')
 result=b'extern "C" void bbd_census_begin(const void*) noexcept;\nextern "C" void bbd_census_tap(const void*,long long,float,float,float,float,float,float,float) noexcept;\n'+raw
 return result.replace(entry,entry+b'        bbd_census_begin(this);\n').replace(material,material+b'        bbd_census_tap(this,cursor,y0,y1,y2,y3,c1,c2,c3);\n')
def check_records(counts,profile,provenance,sources):
 need(provenance['baseline_commit']=='e9e8175f4278d2457279fcb8809cbba1ddb7be2b','baseline')
 need(len(sources)==193 and sources['core/BbdChorus.h']==provenance['stock_BbdChorus_header_sha256'],'full source identity')
 need(provenance['stock_archive_sha256']=='323db1f6f973508b9192f310f4bef66efd45e6c6178afc164dd5819b72b5e314','real stock archive')
 need(set(counts['parity'])=={'factory-held','factory-moving','panel-moving','pure-held'},'four scenes')
 f=counts['fixture'];need((f['sample_rate'],f['block_frames'],f['oversampling'],f['held_voices'],f['callbacks'])==(48000,64,2,8,385) and f['frame_before_midi_every_block'] and f['chorus_on'] is False,'actual fixture')
 c=counts['common_counts_for_every_scene'];need(c['shared_tap_passes_not_per_chain'] and c['downstream_chains']==2,'shared chain accounting')
 for key,passes in {'all':49280,'startup':1024,'middle':31744,'steady':16384,'continuation':128}.items():
  x=c['buckets'][key]
  need(x['passes']==passes and x['taps']==7*passes and x['adjacent_same_cursor']==x['eligible_rebuild_reuses']==3*passes and x['bad']==0,'window totals')
  need(x['pass_hit_histogram']==[0,0,0,passes,0,0,0] and x['per_tap_hits']==[0,0,passes,0,passes,0,passes],'actual tap association')
 need(counts['whole_four_scene_totals']=={'passes':197120,'taps':1379840,'eligible_adjacent_repeats':591360},'whole totals')
 for row in counts['parity'].values():need(row['bytes_per_arm']==5203660 and row['stock_observed_equal'],'fresh parity')
 need(counts['compiles_and_links']==5 and counts['fresh_renderer_invocations']==8 and counts['all_command_exits_zero'],'recorded invocations')
 need(counts['candidate_DSP_runs']==counts['timing_runs']==counts['native_plugin_loads']==0,'stock-only scope')
 expected={("01-s0-p1999",59,4970),("02-s1-p5003",32,3522),("03-s2-p1999",42,4997),("04-s0-p5003",49,3586),("05-s1-p1999",32,4994),("06-s2-p5003",29,3544)}
 need({(r['capture'],r['self_samples'],r['total_samples'])for r in profile['rows']}==expected,'six actual profile proportions')
 for r in profile['rows']:need(r['descriptive_percent']==100*r['self_samples']/r['total_samples'],'profile arithmetic')
 need(profile['descriptive_min_percent']==min(r['descriptive_percent']for r in profile['rows']) and profile['descriptive_max_percent']==max(r['descriptive_percent']for r in profile['rows']),'descriptive range')
 need(profile['confidence_bound'] is False and profile['speed_forecast'] is False and profile['new_sampling_runs']==0,'no performance inference')
def check_package(root=ROOT):
 m=load(root/'manifest.json');need(m['schema']==1,'manifest schema')
 files={p.relative_to(root).as_posix():p for p in root.rglob('*')if p.is_file()}
 need(not any(p.is_symlink()for p in root.rglob('*')),'package symlink')
 need(set(files)==set(m['files'])|{'manifest.json'},'package file set')
 for name,entry in m['files'].items():
  q=PurePosixPath(name);need(not q.is_absolute() and '..'not in q.parts and str(q)==name,'safe manifest path')
  data=files[name].read_bytes();need(len(data)==entry['bytes'] and SHA(data)==entry['sha256'],'package digest: '+name)
 p=load(root/'provenance.json');s=load(root/'stock-sources.json')
 check_records(load(root/'counts.json'),load(root/'profile.json'),p,s)
 for filename,key in [('Observer.cpp','observer_sha256'),('driver.cpp','driver_sha256'),('observer.patch','observer_patch_sha256')]:need(SHA(files[filename].read_bytes())==p[key],'observed-source binding')
 return p,s
def check_repo(repo,p,s):
 src=repo/'src';need(src.is_dir()and not src.is_symlink(),'src directory')
 need(not any(x.is_symlink()for x in src.rglob('*')),'source symlink')
 actual={x.relative_to(src).as_posix():SHA(x.read_bytes())for x in src.rglob('*')if x.is_file()}
 need(actual==s,'193 exact source files')
 for label in ['cost','fixture','clock_header']:
  path=repo/p['committed_inputs'][label];need(SHA(path.read_bytes())==p['committed_inputs'][label+'_sha256'],'committed '+label)
 need(SHA(observed_header((src/'core/BbdChorus.h').read_bytes()))==p['observed_BbdChorus_header_sha256'],'exact observed header reconstruction')
 raw=(repo/p['committed_inputs']['cost']).read_bytes();needle=b'int main(int argc, char** argv) {'
 need(raw.count(needle)==1,'workload main site')
 retained=raw.replace(needle,b'int retained_cost_main(int argc, char** argv) {')
 need(SHA(retained)==p['retained_cost_sha256'],'exact retained workload reconstruction')
 return retained
if __name__=='__main__':
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--repo',type=Path);a=ap.parse_args()
 try:
  p,s=check_package()
  if a.repo:check_repo(a.repo,p,s)
  print('PASS: offline BBD package/receipts'+('; 193-file source and observed-header reconstruction'if a.repo else '')+'. No DSP or raw captures read.')
 except (ValueError,KeyError,OSError,TypeError)as e:print('FAIL:',e,file=sys.stderr);sys.exit(1)
