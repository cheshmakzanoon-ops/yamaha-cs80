#!/usr/bin/env python3
"""Collect bounded work on genuine VST3 callbacks; recompare only verified matching acquisitions. Never time a plug-in."""
import argparse, concurrent.futures as F, hashlib, json, os, pathlib, re, subprocess, sys
P=pathlib.Path
SCENES=('fresh-quiet','held-short','varied-held-short','varied-wake-short')
STATUS={'first4':4,'all5':5,'fifth':1,'duplicate':4,'native':4}
PUBLIC=('pcm.bin','fp.bin','initial.state','postwarm.state','window.state','continuation.state')
def digest(p):return hashlib.file_digest(p.open('rb'),'sha256').hexdigest()
def need(b,m):
    if not b:raise ValueError(m)
def dumpj(p,obj):p.write_text(json.dumps(obj,sort_keys=True,indent=2)+'\n')
def getj(p):return json.loads(p.read_text())
def parse(p,expected):
    rows=p.read_text().splitlines(); names={}; wrapper='cs80_counted_process'; edge='non-virtual thunk to juce::JuceVST3Component::process(Steinberg::Vst::ProcessData&)'
    def scalar(tag):
        v=[l[(len(tag)+1):].strip() for l in rows if l.startswith(tag+':')];need(len(v)==1,'duplicate or missing '+tag);return v[0]
    need(scalar('creator')=='callgrind-3.24.0' and 'desc: Trigger: Client Request: selected_callbacks' in rows,'wrong dump tool/region');need(scalar('events').split()[:3]==['Ir','Dr','Dw'],'event order')
    totals=list(map(int,scalar('totals').split()[:3]));need(totals==list(map(int,scalar('summary').split()[:3])),'counter conservation')
    pat=re.compile(r'(c?fn)=\((\d+)\)(?: (.+))?$');fn=None;target=None;own={};edges={}
    for l in rows:
        m=pat.fullmatch(l)
        if m and m[3]:need(m[2] not in names or m[3]==names[m[2]],'inconsistent symbol');names[m[2]]=m[3]
    i=0
    while i<len(rows):
        l=rows[i];m=pat.fullmatch(l)
        if m:
            if m[1]=='fn':fn=names[m[2]];target=None
            else:target=names[m[2]]
        elif l.startswith('calls='):
            n=int(l[6:].split()[0]);i+=1;d=list(map(int,(rows[i].split()[1:]+['0']*3)[:3]));g=edges.setdefault((fn,target),[0,0,0,0]);
            for j,v in enumerate([n]+d):g[j]+=v
            target=None
        elif fn and l and l[0] in '-+*0123456789':
            d=list(map(int,(l.split()[1:]+['0']*3)[:3]));g=own.setdefault(fn,[0,0,0]);
            for j,v in enumerate(d):g[j]+=v
        i+=1
    g=edges.get((wrapper,edge),[]);need(len(g)==4 and g[0]==expected,'wrong number of real VST3 entries');stub=own.get(wrapper)
    need(stub==[expected*5,expected*3,expected] and all(g[n+1]+stub[n]==totals[n] for n in range(3)),'unisolated callback counter');
    return {'counts':g[1:],'self':sorted(([name]+v for name,v in own.items()),key=lambda row:-row[1])[:45]}
def take(a):
    out=a.out.resolve();out.mkdir();mod=a.module.resolve(strict=True);host=a.host.resolve(strict=True);vg=a.valgrind.resolve(strict=True)
    roles=('native','first4','duplicate','all5','fifth') if a.certify else ('native','first4');scenes=a.scenes or SCENES
    pfx,wm=(256//a.frames,2048//a.frames) if a.align_musical_window else (4,32)
    manifest={'module':digest(mod),'host':digest(host),'role_plan':list(roles),'scenes':list(scenes),'metrics':['Ir','Dr','Dw'],'actual_callbacks_per_history':wm+pfx+2,'warm_callbacks':wm,'counted_prefix_callbacks':pfx,'certify':a.certify,'timing':False,'sample_rate_hz':48000,'frames':a.frames};dumpj(out/'plan.json',manifest)
    args_vg=[str(vg),'--tool=callgrind','--instr-atstart=no','--collect-atstart=no','--toggle-collect=cs80_counted_process','--cache-sim=yes','--I1=32768,8,64','--D1=32768,8,64','--LL=8388608,16,64'];
    def run(item):
        scene,role=item;tag=scene+'-'+role;cap=out/tag;home=out/('home-'+tag);home.mkdir();kind='first4' if role in ('native','duplicate') else role;cmd=[str(host),str(mod),scene,kind,str(cap)];
        if role!='native':cmd=args_vg+['--callgrind-out-file='+str(out/('cg-'+tag)),'--log-file='+str(out/('vg-'+tag+'.log'))]+cmd
        env=dict(os.environ,HOME=str(home),XDG_CONFIG_HOME=str(home/'config'),XDG_CACHE_HOME=str(home/'cache'),LC_ALL='C',TZ='UTC');
        with (out/('stdout-'+tag)).open('wb') as log:subprocess.run(cmd,env=env,check=True,stdout=log,stderr=subprocess.STDOUT,timeout=300)
    with F.ThreadPoolExecutor(max_workers=a.jobs) as pool:list(pool.map(run,((s,r) for r in roles for s in scenes)))
    need(manifest['module']==digest(mod) and manifest['host']==digest(host),'inputs changed');reduce(out)
def reduce(out):
    plan=getj(out/'plan.json');result=dict(plan);result['rows']={};pfx=plan.get('counted_prefix_callbacks',4);wm=plan.get('warm_callbacks',32);total=wm+pfx+2;status={'first4':pfx,'all5':pfx+1,'fifth':1,'duplicate':pfx,'native':pfx}
    for scene in plan['scenes']:
        row={};finger={};stats={}
        for role in plan['role_plan']:
            cap=out/(scene+'-'+role);actual=getj(cap/'outcome.json');need(actual['genuine_calls']==total and actual['counted_calls']==status[role] and actual['warm']==wm and actual['window']==pfx+1 and actual['scene']==scene and actual['note_event_reads']==(0 if scene=='fresh-quiet' else 8),'bad scene ledger');need(bool(actual['under_valgrind'])==(role!='native'),'runtime mismatch');need((actual['window_peak']<=1e-6) if scene=='fresh-quiet' else (actual['window_peak']>1e-5),'vacuous fixture');need(all((cap/f).stat().st_size>0 for f in PUBLIC),'empty evidence');need((cap/'pcm.bin').stat().st_size==plan.get('frames',64)*total*2*4,'wrong host audio buffer'); fp={f:digest(cap/f) for f in PUBLIC};
            if role=='native':finger['native']=fp
            else:
                if 'guest' not in finger:finger['guest']=fp
                need(fp==finger['guest'],'same-runtime public capture changed');c=parse(out/('cg-'+scene+'-'+role+'.1'),status[role]);row[role]=c['counts'];
                if role=='first4':stats=c['self']
        need(all(finger['native'][f]==finger['guest'][f] for f in PUBLIC if f!='fp.bin'),'cross-runtime PCM/state mismatch on same build');
        if plan['certify']:need(row['duplicate']==row['first4'] and row['all5']==[a+b for a,b in zip(row['first4'],row['fifth'])] and all(x>0 for x in row['fifth']),'duplicate and exact additivity control')
        result['rows'][scene]={'guest_work':row,'public_sha256':finger,'largest_self':stats}
    dumpj(out/'receipt.json',result);return result
def compare(a):
    base=reduce(a.baseline);cand=reduce(a.candidate);need(base['certify'] and base['host']==cand['host'] and base.get('frames',64)==cand.get('frames',64),'uncertified baseline or unmatched host');small={}
    for s,r in cand['rows'].items():
        stock=base['rows'][s];need(stock['public_sha256']==r['public_sha256'],'stock/candidate PCM, FP or state mismatch: '+s)
        x,y=stock['guest_work']['first4'],r['guest_work']['first4'];small[s]={'stock':x,'candidate':y,'delta':[b-a for a,b in zip(x,y)],'percent_change':[100*(b-a)/a for a,b in zip(x,y)],'native_and_guest_public_sha256':r['public_sha256'],'stock_controls':stock['guest_work'],'candidate_controls':r['guest_work']}
    result={'stock_module_sha256':base['module'],'candidate_module_sha256':cand['module'],'host_sha256':base['host'],'metrics':base['metrics'],'frames':base.get('frames',64),'sample_rate_hz':48000,'warm_stereo_frames':base.get('warm_callbacks',32)*base.get('frames',64),'counted_prefix_callbacks':base.get('counted_prefix_callbacks',4),'counted_prefix_stereo_frames':base.get('frames',64)*base.get('counted_prefix_callbacks',4),'controls_legend':{'first4':'counted prefix','all5':'prefix plus the next actual call','fifth':'only the next call','duplicate':'repeated prefix'},'scenes':small,'timing':False,'physical_cpu_budget_verified':False,'candidate_duplicate_and_additive_controls':cand['certify']};dumpj(a.out,result);print(json.dumps({s:r['percent_change'] for s,r in small.items()}))
if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__);sub=ap.add_subparsers(dest='op',required=True);sp=sub.add_parser('take');sp.add_argument('module',type=P);sp.add_argument('out',type=P);sp.add_argument('--host',required=True,type=P);sp.add_argument('--valgrind',required=True,type=P);sp.add_argument('--jobs',type=int,default=2);sp.add_argument('--frames',type=int,choices=(16,32,64),default=64);sp.add_argument('--align-musical-window',action='store_true');sp.add_argument('--certify',action='store_true');sp.add_argument('--scenes',nargs='+',choices=SCENES);sp=sub.add_parser('compare');sp.add_argument('baseline',type=P);sp.add_argument('candidate',type=P);sp.add_argument('out',type=P);a=ap.parse_args();take(a) if a.op=='take' else compare(a)
