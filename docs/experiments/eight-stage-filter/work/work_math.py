import collections
def validate(stock,candidate,full=True):
 assert len(stock)==17408
 for rows in (stock,candidate):
  assert all(1024<=x['block']<=1040 and x['n'] in (4,8) and x['ok'] in (0,1) for x in rows)
  assert [x['block'] for x in rows]==sorted(x['block'] for x in rows)
  for x in rows:
   n=x['n'];assert len(x['lanes'])==n and len({v[0] for v in x['lanes']})==n
   assert all(0<=v[0]<32 for v in x['lanes'])
   if full: assert all(len(v)==3 and 0<=v[1]<2**32 and 0<=v[2]<2**64 for v in x['lanes'])
   assert x['passes'] in range(6) and x['updates']==max(0,x['passes']-1)
   masks=[(x['masks']>>(8*j))&255 for j in range(x['passes'])]
   assert x['masks']>>(8*x['passes'])==0
   assert not masks or masks[0]==(1<<n)-1
   assert all(m and m<1<<n for m in masks)
   assert all((b&a)==b for a,b in zip(masks,masks[1:]))
   assert bool(x['passes'])==bool(x['ok']) # retained observation, not a universal recovery guarantee
 pairs=[];pos=0;declines=[]
 for x in candidate:
  width=x['n']//4;expected=stock[pos:pos+width];assert len(expected)==width
  assert all(y['n']==4 and y['block']==x['block'] for y in expected)
  assert sum((y['lanes'] for y in expected),[])==x['lanes']
  if x['n']==8 and not x['ok']:
   declines.append({'block':x['block'],'stock_quartet_index':pos,'stock_statuses':[y['ok'] for y in expected]});continue
  if x['n']==4:
   assert all(x[k]==expected[0][k] for k in ['ok','passes','updates','masks'])
  else:
   assert all(y['ok'] for y in expected)
   assert x['passes']==max(y['passes'] for y in expected)
   assert x['updates']==max(y['updates'] for y in expected)
   for half,y in enumerate(expected):
    for j in range(x['passes']):
     actual=(x['masks']>>(8*j+4*half))&15
     wanted=(y['masks']>>(8*j))&15 if j<y['passes'] else 0
     assert actual==wanted
   pairs.append({'block':x['block'],'stock_quartet_index':pos,'four_passes':[y['passes'] for y in expected],'eight_passes':x['passes']})
  pos+=width
 assert pos==len(stock)
 return pairs,declines

def totals(rows):
 return {'kernel_calls':len(rows),'accepted':sum(x['ok'] for x in rows),'declined':sum(not x['ok'] for x in rows),
 'loop_body_passes':sum(x['passes'] for x in rows),'newton_update_passes':sum(x['updates'] for x in rows),
 'width_weighted_loop_slots':sum(x['n']*x['passes'] for x in rows),
 'active_lane_loop_slots':sum(sum(((x['masks']>>(8*j))&255).bit_count() for j in range(x['passes'])) for x in rows),
 'histogram':dict(sorted(collections.Counter(f"{x['n']}-wide/{'accepted' if x['ok'] else 'declined'}/{x['passes']}-passes" for x in rows).items()))}

