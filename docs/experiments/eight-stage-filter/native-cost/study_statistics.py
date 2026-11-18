#!/usr/bin/env python3
"""Frozen average-CPU statistics. Pure offline arithmetic: no clocks or DSP."""
import hashlib
import math
from collections import Counter

LABELS = ('S0', 'S1', 'C', 'S+')
METRICS = ('placebo', 'extra', 'candidate_pooled', 'candidate_S0', 'candidate_S1')
DRAWS = 20000
SEED = 'cs80-eight-native-average-v1/bootstrap/0x80305125'
T95_DF5 = 2.570581835636305
PLAN = {
    'schema': 1, 'scope': '48kHz/64-frame held-eight-voice real-VST3 average thread CPU only',
    'blocks': 24, 'processes': 96, 'williams_sets': 6, 'blocks_per_set': 4,
    'outcome': 'unnormalized sum of actual process() thread-CPU nanoseconds; all arms use512-callback baseline denominator',
    'block_contrasts': {'placebo': 'log(S1/S0)', 'extra': '(log(S+/S0)+log(S+/S1))/2',
        'candidate_pooled': '(log(S0/C)+log(S1/C))/2', 'candidate_S0': 'log(S0/C)', 'candidate_S1': 'log(S1/C)'},
    'estimator': 'equal-weight mean of complete block log contrasts',
    'bootstrap': {'unit': 'one complete four-treatment block, all five contrasts resampled jointly',
        'draws': DRAWS, 'seed': SEED, 'group_keys': ['all','first12','last12','C-position-0','C-position-1','C-position-2','C-position-3','S1-before-S0','S1-after-S0'],
        'rng': 'SHA256(seed + slash + group_key + slash + decimal_counter), big-endian256-bit integer, reject >=2^256-(2^256 mod n), then mod n; one draw per sampled block index',
        'quantile': 'linear interpolation at (draws-1)*p; p=.025,.975',
        'constant_columns': 'exact constant mean/interval; all-constant group skips identical resamples'},
    'set_sensitivity': {'mandatory': True, 'unit': 'six complete four-block Williams-set mean log contrasts',
        'interval': 'mean +/- t(.975,df5)*sample_sd/sqrt(6)', 't_critical': T95_DF5,
        'gates': 'main placebo, extra and all three candidate comparisons must pass BOTH block-bootstrap and set-t intervals',
        'leave_one_set': 'all six omission point estimates reported descriptively; never select an omission'},
    'transform': {'placebo_extra': '100*expm1(log contrast)', 'candidate_saving': '-100*expm1(-log contrast)'},
    'gate_numeric_precision': 'round transformed percentage or percentage-point difference to12 decimal places before comparison',
    'main_gates': {'placebo_entire_95_percent': [-2,2], 'extra_entire_95_percent': [3,10],
        'candidate_lower_95_saving_strictly_above_percent': 3, 'candidate_comparisons': ['pooled','S0','S1'],
        'extra_work_target_percent': 6.25, 'extra_target_is_exact_CPU_null': False},
    'split_gates': {'first12_and_last12_candidate_pooled_bootstrap_lower_95_strictly_above':0,
        'absolute_point_saving_gap_pp_max':3},
    'order_gates': {'candidate_by_position_each_point_strictly_above_percent':0,
        'candidate_position_point_range_pp_max':5,
        'placebo_each_twin_temporal_order_point_inside_percent':[-2,2]},
    'scope_exclusions': ['deadline or tail-latency admission','callback-level independent replicates','96 independent treatment comparisons','other scenes or machines','repairing old failed studies'],
    'policy': 'all96 chronological processes and24 complete blocks required; all gates AND; no exclusion, retries, replacement, subset admission or post-outcome rule changes',
    'correlation_limit': 'set-t allows within-set correlation; six-set independence and t approximation are not guarantees against arbitrary whole-VM temporal correlation',
}


def need(ok, message):
    if not ok:
        raise ValueError(message)


def isint(value, positive=False):
    return type(value) is int and (value > 0 if positive else value >= 0)


def mean(values):
    return math.fsum(values) / len(values)


def effect(metric, value):
    return -100 * math.expm1(-value) if metric.startswith('candidate') else 100 * math.expm1(value)


def decide(value):
    return round(value, 12)


class Rng:
    def __init__(self, group):
        self.prefix = SEED + '/' + group + '/'
        self.counter = 0

    def below(self, n):
        limit = 2**256 - (2**256 % n)
        while True:
            raw = hashlib.sha256((self.prefix + str(self.counter)).encode()).digest()
            self.counter += 1
            value = int.from_bytes(raw, 'big')
            if value < limit:
                return value % n


def quantile(sorted_values, p):
    x = (len(sorted_values) - 1) * p
    i = int(x)
    return sorted_values[i] + (sorted_values[min(i+1,len(sorted_values)-1)] - sorted_values[i]) * (x-i)


def bootstrap(vectors, group, draws=DRAWS):
    need(vectors and all(len(v)==5 for v in vectors), 'invalid bootstrap vectors')
    n = len(vectors)
    columns = list(zip(*vectors))
    constants = [all(x==col[0] for x in col) for col in columns]
    if all(constants):
        return [(col[0],col[0]) for col in columns]
    rng = Rng(group)
    values = [[] for _ in METRICS]
    for _ in range(draws):
        selected = [vectors[rng.below(n)] for _ in range(n)]
        for k in range(5):
            values[k].append(columns[k][0] if constants[k] else math.fsum(v[k] for v in selected)/n)
    return [(quantile(sorted(v),.025),quantile(sorted(v),.975)) for v in values]


def summary(blocks, key):
    vectors = [b['contrasts'] for b in blocks]
    intervals = bootstrap(vectors,key)
    result = {'blocks':[b['block'] for b in blocks], 'n_blocks':len(blocks), 'metrics':{}}
    for k,name in enumerate(METRICS):
        m = mean([v[k] for v in vectors]);lo,hi=intervals[k]
        result['metrics'][name] = {'mean_log':m,'point_percent':effect(name,m),
            'bootstrap_95_log':[lo,hi], 'bootstrap_95_percent':[effect(name,lo),effect(name,hi)]}
    return result


def set_sensitivity(blocks):
    sets = []
    for rep in range(6):
        bs = [b for b in blocks if b['replicate']==rep]
        need(len(bs)==4,'missing complete Williams set')
        sets.append({'replicate':rep,'blocks':[b['block'] for b in bs],
            'mean_log':[mean([b['contrasts'][k] for b in bs]) for k in range(5)]})
    result={'sets':sets,'df':5,'t_critical':T95_DF5,'metrics':{},'leave_one_complete_set':[]}
    for k,name in enumerate(METRICS):
        vs=[s['mean_log'][k] for s in sets];m=mean(vs)
        sd=math.sqrt(math.fsum((x-m)**2 for x in vs)/5)
        half=T95_DF5*sd/math.sqrt(6);lo,hi=m-half,m+half
        result['metrics'][name]={'mean_log':m,'sample_sd_log':sd,'set_t95_log':[lo,hi],
            'point_percent':effect(name,m),'set_t95_percent':[effect(name,lo),effect(name,hi)]}
    for omitted in range(6):
        result['leave_one_complete_set'].append({'omitted_set':omitted,'descriptive_only':True,
            'point_percent':{name:effect(name,mean([s['mean_log'][k] for s in sets if s['replicate']!=omitted])) for k,name in enumerate(METRICS)}})
    return result


def schedule_audit(schedule):
    slots,blocks=schedule['slots'],schedule['blocks']
    need(len(slots)==96 and len(blocks)==24,'schedule counts')
    need([s['slot'] for s in slots]==list(range(96)),'schedule chronological slot IDs')
    need([b['block'] for b in blocks]==list(range(24)),'schedule chronological block IDs')
    need(Counter(s['treatment'] for s in slots)=={t:24 for t in LABELS},'treatment counts')
    for rep in range(6):
        rs=[s for s in slots if s['replicate']==rep]
        need(len(rs)==16 and {s['block'] for s in rs}==set(range(rep*4,rep*4+4)),'complete contiguous set')
        for pos in range(4):
            need(Counter(s['treatment'] for s in rs if s['position']==pos)=={t:1 for t in LABELS},'within-set position balance')
        adjacent=Counter()
        for block in range(rep*4,rep*4+4):
            ss=[s for s in rs if s['block']==block]
            need([s['position'] for s in ss]==list(range(4)) and {s['treatment'] for s in ss}==set(LABELS),'complete ordered block')
            b=blocks[block]
            need(b['labels']==[s['treatment'] for s in ss] and b['slot_ids']==[s['slot'] for s in ss] and b['replicate']==rep,'block and slots differ')
            adjacent.update(zip(b['labels'],b['labels'][1:]))
        need(adjacent=={(a,b):1 for a in LABELS for b in LABELS if a!=b},'within-set ordered adjacency balance')
    for s in slots:
        need(s['module_role']==('candidate' if s['treatment']=='C' else 'stock'),'wrong physical role')
        need(s['warm_callbacks']==1024 and s['measured_callbacks']==(544 if s['treatment']=='S+' else 512) and s['normalization_callbacks']==512,'wrong callback or normalization count')
    return {'sets':6,'blocks':24,'slots':96,'each_treatment_each_position':6,
            'each_directed_distinct_within_block_adjacency':6,'each_half_balanced':True,
            'between_block_transitions':dict(Counter(a['labels'][-1]+'>'+b['labels'][0] for a,b in zip(blocks,blocks[1:])))}


def validate_rows(rows,schedule):
    balance=schedule_audit(schedule)
    need(type(rows) is list and len(rows)==96,'all96 observations required; no subset')
    keys=set(schedule['slots'][0])|{'cpu_ns','pid','module_sha256','integrity_pass'}
    pids=set();stock=set();candidate=set()
    for i,(row,slot) in enumerate(zip(rows,schedule['slots'])):
        need(type(row) is dict and set(row)==keys,'row schema '+str(i))
        need(all(type(row[k]) is type(v) and row[k]==v for k,v in slot.items()),'slot/role/order mismatch '+str(i))
        need(isint(row['cpu_ns'],True),'CPU must be positive raw integer nanoseconds')
        need(isint(row['pid'],True) and row['pid'] not in pids,'fresh unique process required')
        pids.add(row['pid']);need(row['integrity_pass'] is True,'failed integrity cannot be analyzed for admission')
        h=row['module_sha256'];need(type(h) is str and len(h)==64 and all(c in '0123456789abcdef' for c in h),'bad module hash')
        (candidate if row['module_role']=='candidate' else stock).add(h)
    need(len(stock)==len(candidate)==1 and stock!=candidate,'physical stock twins/positive or genuine candidate identity mismatch')
    return balance


def evaluate_gates(main,sets,subgroups):
    gates=[]
    def add(code,ok,value):gates.append({'gate':code,'pass':bool(ok),'observed':value})
    for label,container,key in [('block-bootstrap',main,'bootstrap_95_percent'),('six-set-t',sets,'set_t95_percent')]:
        p=container['metrics']['placebo'][key];e=container['metrics']['extra'][key]
        add(label+'.placebo-equivalence',decide(p[0])>=-2 and decide(p[1])<=2,p)
        add(label+'.known-extra-sensitivity',decide(e[0])>=3 and decide(e[1])<=10,e)
        for metric in METRICS[2:]:
            ci=container['metrics'][metric][key]
            add(label+'.'+metric+'.lower-above3',decide(ci[0])>3,ci)
    halves=[subgroups[x]['metrics']['candidate_pooled'] for x in ('first12','last12')]
    for label,result in zip(('first12','last12'),halves):
        add('split.'+label+'.positive-lower',decide(result['bootstrap_95_percent'][0])>0,result['bootstrap_95_percent'])
    gap=abs(halves[0]['point_percent']-halves[1]['point_percent'])
    add('split.point-gap-at-most3pp',decide(gap)<=3,gap)
    positions=[subgroups['C-position-'+str(i)]['metrics']['candidate_pooled']['point_percent'] for i in range(4)]
    for i,value in enumerate(positions):add('order.C-position-'+str(i)+'.positive-point',decide(value)>0,value)
    spread=max(positions)-min(positions);add('order.C-position-range-at-most5pp',decide(spread)<=5,spread)
    for label in ('S1-before-S0','S1-after-S0'):
        value=subgroups[label]['metrics']['placebo']['point_percent']
        add('order.'+label+'.placebo-point',-2<=decide(value)<=2,value)
    return gates


def analyze(rows,schedule):
    balance=validate_rows(rows,schedule);blocks=[]
    for b in range(24):
        rr=rows[b*4:b*4+4];d={r['treatment']:r for r in rr};v={k:r['cpu_ns'] for k,r in d.items()}
        g0=math.log(v['S0']/v['C']);g1=math.log(v['S1']/v['C'])
        contrasts=[math.log(v['S1']/v['S0']),.5*(math.log(v['S+']/v['S0'])+math.log(v['S+']/v['S1'])),.5*(g0+g1),g0,g1]
        blocks.append({'block':b,'replicate':rr[0]['replicate'],'slot_ids':[r['slot'] for r in rr],
            'order':[r['treatment'] for r in rr],'cpu_ns':v,'contrasts':contrasts,
            'individual_effect_percent':{m:effect(m,x) for m,x in zip(METRICS,contrasts)},
            'candidate_position':d['C']['position'],'S1_before_S0':d['S1']['position']<d['S0']['position']})
    main=summary(blocks,'all');sets=set_sensitivity(blocks)
    groups={'first12':blocks[:12],'last12':blocks[12:]}
    groups.update({'C-position-'+str(i):[b for b in blocks if b['candidate_position']==i] for i in range(4)})
    groups.update({name:[b for b in blocks if b['S1_before_S0']==flag] for name,flag in [('S1-before-S0',True),('S1-after-S0',False)]})
    need(all(len(v)==(6 if k.startswith('C-position') else 12) for k,v in groups.items()),'subgroup design balance')
    subgroups={key:summary(value,key) for key,value in groups.items()}
    gates=evaluate_gates(main,sets,subgroups)
    calibrated=all(g['pass'] for g in gates if 'placebo' in g['gate'] or 'known-extra' in g['gate'])
    admitted=all(g['pass'] for g in gates)
    return {'schema':1,'scope':PLAN['scope'],'status':'ADMITTED for this frozen average-CPU workload only' if admitted else 'NO ADMISSION',
        'admission':admitted,'calibration_pass':calibrated,'all_gates_required':True,
        'sampling_units':{'blocks':24,'whole_set_sensitivity_units':6,'processes_observed':96,'callbacks_are_replicates':False},
        'balance':balance,'main':main,'set_sensitivity':sets,'subgroups':subgroups,
        'gates':gates,'failures':[g for g in gates if not g['pass']],
        'blocks':blocks,'observations':rows,'statistical_plan':PLAN,
        'disposition':'A calibration failure is inconclusive, not evidence of an admitted candidate gain. No subset, retry, old-study rescue or deadline inference.'}
