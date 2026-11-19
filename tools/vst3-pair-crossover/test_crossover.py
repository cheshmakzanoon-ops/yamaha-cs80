#!/usr/bin/env python3
"""Strictly offline arithmetic and schedule fault controls. No native modules or clocks."""
import copy
import math
import unittest
import crossover as c

A=c.SHIPPING_CUTOFF; B='f'*64

def raw(e=0,j=0,s='idle',role='Q',cf=.90,empty=100,gap=100):
    p=c.plan(e,j,s);roleA=c.orientation(e,j)['a_lane']; ts=100000;cpu=[1234,3456];rs=[]
    for i,l in c.visits(p):
        # Heterogeneous absolute workloads; roles see the same logical inputs
        # and share the same level/cell factor. Extra-index rotation is genuine.
        n=round((99991+e*100+j*200)*(1.0 if l==roleA or role!='C' else cf))
        rs.append([i,l,cpu[l],cpu[l]+n,ts,ts+n+5]);cpu[l]+=n+200;ts+=n+5+gap
    return dict(epoch=e,cpu_cell=j,scene=s,role=role,a_sha256=A,b_sha256=B if role=='C' else A,
                empty_cpu_ns=[[empty]*128 for _ in range(2)],clocks=rs)

def clean_summary(role='Q', factors=None):
    factors=factors or {}
    out=[]
    for e in range(12):
        for j in c.cpu_order(e):
            for s in c.scene_order(e,j):
                a=100000;b=a if role!='C' else round(a*factors.get(s,.9))
                p=c.plan(e,j,s);row=dict(epoch=e,cpu_cell=j,scene=s,role=role,a_sha256=A,b_sha256=B if role=='C' else A,
                                         empty_fraction=.001,q1_gap_multiple=1.1)
                for k,v in [('a',a),('b',b)]:
                    row[k]=576*v;row[k+'_extra']=64*v;row[k+'_common']=512*v
                    for q in (1,4):
                        row[k+'_q'+str(q)]=sum(v for _,qq,_,_ in p if qq==q)
                        row[k+'_common_q'+str(q)]=sum(v for _,qq,_,extra in p if qq==q and not extra)
                out.append(row)
    return out

class Crossover(unittest.TestCase):
    def test_schedule_complete(self):
        plan=c.generate()
        self.assertEqual(48,len(plan['slots']))
        for j,row in enumerate(plan['slots']):
            self.assertEqual(set(c.SCENES),set(row['scene_order']))
        for e in range(12):
            self.assertEqual(sorted(c.cpu_order(e)),[0,1,2,3])
            for j in range(4):
                for s in c.SCENES:
                    p=c.plan(e,j,s)
                    self.assertEqual(sum(x[3] for x in p),64)
                    self.assertEqual(sum(x[1]==1 for x in p),288)
                    seq=list(c.visits(p));self.assertEqual(len(set(seq)),1152)
                    self.assertEqual([i for i,l in seq if l==0],list(range(576)))
                    self.assertEqual([i for i,l in seq if l==1],list(range(576)))
    def test_affinities(self):
        self.assertEqual(c.cpu_ids(range(9)),([0,2,4,6],1))
        with self.assertRaises(ValueError):c.cpu_ids([0,1,2,3])
    def test_reduction(self):
        r=c.reduced(raw());self.assertEqual(r['b']/r['a_common'],576/512)
        self.assertEqual(r['empty_fraction'],100/99991);self.assertLess(r['q1_gap_multiple'],3)
    def test_handoff_permutation(self):
        r=raw();r['clocks'][0][4:6],r['clocks'][1][4:6]=r['clocks'][1][4:6],r['clocks'][0][4:6]
        with self.assertRaisesRegex(ValueError,'baton'):c.reduced(r)
    def test_missing_or_duplicate_row(self):
        for bad in ('lost','duplicate'):
            r=raw()
            if bad=='lost':r['clocks'].pop()
            else:r['clocks'][1]=r['clocks'][0]
            with self.assertRaises(ValueError):c.reduced(r)
    def test_missing_fresh_epochs(self):
        x=clean_summary()[:-1]
        with self.assertRaises(ValueError):c.assess(x,'Q',A,A)
    def test_study_cell_reordering(self):
        x=clean_summary();x[0],x[1]=x[1],x[0]
        with self.assertRaisesRegex(ValueError,'reordered'):c.assess(x,'Q',A,A)
    def test_control_pass_genuine_extra(self):
        d=c.assess(clean_summary(),'Q',A,A)
        self.assertTrue(d['numerical_gates_pass']);self.assertAlmostEqual(d['scenes']['idle']['extra_work']['point_pct'],12.5)
    def test_positive_control_not_divided_by_extra_callbacks(self):
        x=clean_summary()
        for r in x:r['b']=r['b_common']  # adversarially erase every bit of extra measured duration
        self.assertFalse(c.assess(x,'Q',A,A)['numerical_gates_pass'])
    def test_extra_callback_not_qualified_by_null_crossing(self):
        x=clean_summary()
        for r in x:
            # Exchange zero-mean epoch biases; the interval includes true zero, but is NOT equivalent.
            r['b_common']*=math.exp(.16*(-1)**r['epoch'])
        self.assertFalse(c.assess(x,'Q',A,A)['gates']['idle:twins-equivalence'])
    def test_not_fooled_by_active_regression(self):
        result=c.assess(clean_summary('C',{'held_varied':1.035}),'C',A,B)
        self.assertFalse(result['numerical_gates_pass']);self.assertTrue(result['gates']['idle:primary'])
        self.assertFalse(result['gates']['held_varied:nonregression'])
    def test_varied_release_scope(self):
        result=c.assess(clean_summary('C',{'release_varied':1.01}),'C',A,B)
        self.assertTrue(result['numerical_gates_pass'])
        self.assertLess(result['scenes']['release_varied']['saving']['ci95_pct'][0],0)
    def test_exact_five_does_not_pass_strict_gate(self):
        x=clean_summary('C',{'idle':.95})
        self.assertFalse(c.assess(x,'C',A,B)['gates']['idle:primary'])
    def test_another_elf_is_a_different_study(self):
        with self.assertRaisesRegex(ValueError,'shipping baseline'):c.assess(clean_summary(),'Q','a'*64,'a'*64)
    def test_point_estimate_is_not_enough(self):
        x=clean_summary('C')
        for r in x:r['b']*=math.exp(.15*(-1)**r['epoch'])
        self.assertFalse(c.assess(x,'C',A,B)['gates']['idle:primary'])
    def test_switching_dependence(self):
        x=clean_summary('C')
        for r in x:
            if r['scene']=='idle':r['b_q4']=r['a_q4']
        self.assertFalse(c.assess(x,'C',A,B)['gates']['idle:quantum-agreement'])
    def test_wrong_binary(self):
        x=clean_summary();x[0]['b_sha256']=B
        with self.assertRaisesRegex(ValueError,'module'):c.assess(x,'Q',A,A)
    def test_many_empty_ticks_or_large_gap(self):
        for field,val in [('empty_fraction',.04),('q1_gap_multiple',3.1)]:
            x=clean_summary();x[0][field]=val
            self.assertFalse(c.assess(x,'Q',A,A)['numerical_gates_pass'])

if __name__=='__main__':unittest.main()
