from pathlib import Path
import re,math,json,hashlib
p=Path(__file__).resolve().parent
scale=[0,100,300,500,800]
def cents(degree):
 period,within=divmod(degree,5)
 return 1200*period+scale[within]
result={}
for name,offset in [('negative1',-1),('negative2',-2)]:
 text=(p/(name+'-oracle.txt')).read_text()
 rows={int(n):hz for n,hz in re.findall(r'^\s+(\d+):\s+([\d]+\.[\d]+)\s*[^\d.]',text,re.M)}
 assert set(rows)==set(range(128)),len(rows)
 errors=[]
 for note,hz_text in rows.items():
  hz=float(hz_text)
  repeats,slot=divmod(note-60,2)
  degree=3*repeats+(offset if slot==0 else 0)
  expected=440*2**((cents(degree)-cents(offset))/1200)
  error=abs(hz-expected)
  tolerance=1e-6+1e-10*max(hz,expected)
  assert error<=tolerance,(name,note,error,tolerance)
  errors.append(error)
 result[name]={'mapped_degrees':[offset,0],'frequency_count':len(rows),'matched_degree_first':128,'maximum_absolute_error_hz':max(errors),'notes_56_through_65_hz':[float(rows[n]) for n in range(56,66)]}
# Provenance hashes are recorded, not rehashed: official software is not vendored.
saved=json.loads((p/'negative-map-audit.json').read_text())
for name in result:
 assert result[name]==saved[name],name
print(json.dumps(result,indent=2))
