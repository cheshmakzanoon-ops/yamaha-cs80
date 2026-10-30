import numpy as np,json,pathlib,hashlib
root=pathlib.Path('/tmp/oscillator-nonlinear-review/reference-coarse');N=16384;M=2**19;tau=30e-6;rc=.1034;delay=8
rows=[]
for fs in [88200.,96000.]:
 for target in [1046.,4186.,8372.,16744.,23000.]:
  bin=int(round(target*N/fs));f=fs*bin/N;p=np.arange(M)/M;k=(3.5-.45)/(1-261.6*tau)
  raw=np.where(p<f*tau,3.5,k*(1-p));w=2j*np.pi*np.fft.rfftfreq(M,1/(M*f));hp=w*rc/(1+w*rc)
  u=np.fft.irfft(np.fft.rfft(raw)*hp,n=M);tri=3.2*np.clip(abs(u)/1.525,0,1);x=np.fft.irfft(np.fft.rfft(tri)*hp,n=M)/1.6
  sine=-1.5017767959*np.sign(x)*np.interp(np.clip(abs(x),0,1),np.arange(5)/4,np.sin(np.pi*np.arange(5)/8))
  coeff=np.array([np.fft.rfft(v)/M for v in [-u,np.where(u>0,1.5,-1.5),sine]])
  nh=int(fs*.5/f);ref=np.zeros((N,3));phase=(np.arange(N)+1-delay)*f/fs
  for b in range(3):
   ref[:,b]=coeff[b,0].real
   for h in range(1,nh+1):ref[:,b]+=2*np.real(coeff[b,h]*np.exp(2j*np.pi*h*phase))
  path=root/f'ref-{int(fs)}-{int(target)}.bin';ref.astype('float64').tofile(path)
  rows.append({'fs':fs,'target':target,'bin':bin,'f':f,'internal_samples':N,'period_quadrature_points':M,'internal_nyquist_harmonics':nh,'reference_sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
(root/'reference_manifest.json').write_text(json.dumps({'model':'w3-current-fold-v1; independent denseFFT continuous periodic RC/fold/shaper; ideal lowpass to INTERNAL Nyquist BEFORE sample','delay_internal':delay,'rows':rows},indent=2))
