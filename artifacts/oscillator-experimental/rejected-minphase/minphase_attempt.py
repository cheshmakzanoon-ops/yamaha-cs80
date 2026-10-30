"""Rejected first full-length homomorphic minphase kernel, 2026-10-29.
Reproduces the initial C++ design equations (NumPy FFT instead of radix-2 C++).
This is failed numerical exploration, NOT the committed oscillator algorithm.
"""
import numpy as np,json,hashlib
length=64;phases=64;N=32768;points=length*phases+1;cutoff=.40
n=np.arange(points);t=(n-(points-1)/2)/phases;a=2*np.pi*n/(points-1)
window=.35875-.48829*np.cos(a)+.14128*np.cos(2*a)-.01168*np.cos(3*a)
h=np.zeros(N);h[:points]=2*cutoff*np.sinc(2*cutoff*t)*window/phases
logmag=np.log(np.maximum(1e-12,np.abs(np.fft.fft(h))))
c=np.fft.ifft(logmag);c[1:N//2]*=2;c[N//2+1:]=0
minimum=np.fft.ifft(np.exp(np.fft.fft(c))).real
report={'status':'REJECTED before production integration','method':'full-length real-cepstrum homomorphic; no half-length sqrt conversion','prototype_length_samples':length,'fractional_phases':phases,'fft_size':N,'cutoff_cycles_per_sample':cutoff,'magnitude_floor':1e-12,'unretained_tail_L1':float(abs(minimum[points:]).sum()),'retained_L1':float(abs(minimum[:points]).sum()),'initial_Cpp_reported_unretained_tail_L1':0.070251,'reason':'finite support truncation error and expected nonlinearphase gate risk; not adopted','numpy_version':np.__version__}
print(json.dumps(report,indent=2))
