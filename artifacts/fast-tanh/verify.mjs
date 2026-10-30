// Experimental mathematical candidate only: no Engine integration or native performance claim.
function boundedTanhCandidate(x) {
  if (Number.isNaN(x)) return x;
  const magnitude = Math.abs(x);
  if (magnitude < 2 ** -20) return x;
  if (magnitude >= 20) return x < 0 ? -1 : 1;
  const z = magnitude * 0.25, s = z * z;
  const p = z * (10395 + s * (1260 + 21 * s));
  const q = 10395 + s * (4725 + s * (210 + s));
  const p2 = p * p, q2 = q * q, a = p2 + q2;
  const value = Math.min(1, (4 * p * q * a) / (a * a + 4 * p2 * q2));
  return x < 0 ? -value : value;
}
function runProof() {
  const assert=(b,m)=>{if(!b)throw new Error(m);};
  const multiply=(a,b)=>{const r=Array(a.length+b.length-1).fill(0n);a.forEach((x,i)=>b.forEach((y,j)=>r[i+j]+=x*y));return r;};
  const p6=[0n,10395n,0n,1260n,0n,21n],q6=[10395n,0n,4725n,0n,210n,0n,1n];
  const p7=[0n,135135n,0n,17325n,0n,378n,0n,1n],q7=[135135n,0n,62370n,0n,3150n,0n,28n];
  const a=multiply(p7,q6),b=multiply(p6,q7);
  for(let i=0;i<a.length;i++)assert(a[i]-(b[i]??0n)===(i===13?1n:0n),"convergent determinant");
  const d=1000n,d2=d*d,d4=d2*d2,d6=d4*d2;
  function vals(n){const n2=n*n,n3=n2*n,n4=n2*n2,n5=n4*n,n6=n3*n3;return {
    p:10395n*n*d*d4+1260n*n3*d*d2+21n*n5*d,
    q:10395n*d6+4725n*n2*d4+210n*n4*d2+n6,
    q7:135135n*d6+62370n*n2*d4+3150n*n4*d2+28n*n6
  };}
  let maxN=0n,maxD=1n,interval=0;
  for(let k=0;k<5000;k++){
    const lo=vals(BigInt(k)),hi=vals(BigInt(k+1)),A=lo.p,B=hi.q;
    assert(A>=0n&&A<=B,"derivative interval");
    const A2=A*A,B2=B*B,z=B2-A2,w=B2*B2+6n*A2*B2+A2*A2;
    const N=4n*B2*z*z*z*(BigInt(k+1)**13n);
    const D=d*lo.q*lo.q7*w*w;
    assert(D>0n&&N*10000000000000n<186n*D,"uniform real-arithmetic bound");
    if(N*maxD>maxN*D){maxN=N;maxD=D;interval=k;}
  }
  const K=1n<<53n;
  const upperN=(K+1n)*(K-38n),upperD=(K-35n)*(K-76n);
  const lowerN=(K-70n)*(K-1n)*(K-38n),lowerD=(K-35n)*K*K;
  assert((upperN-upperD)*K<100n*upperD,"upper roundoff budget");
  assert((lowerD-lowerN)*K<100n*lowerD,"lower roundoff budget");
  assert(186n*K+100n*10000000000000n<200n*K,"combined bound <2e-11");
  let factorial=1n;for(let n=2n;n<=60n;n++)factorial*=n;
  let sum=0n,termFactorial=1n,power=1n;
  for(let n=0n;n<=60n;n++){if(n>0n){termFactorial*=n;power*=40n;}sum+=power*(factorial/termFactorial);}
  assert(sum>100000000000000000n*factorial,"exp(40)>1e17 saturation bound");
  assert(100000000000n<6n*(1n<<60n),"tiny-input bound");
  return {intervals:5000,intervalWidthZ:"1/1000",domainX:"[-20,20]",maxIntervalX:[4*interval/1000,4*(interval+1)/1000],certifiedRealAbsoluteError:"<1.86e-11",conditionalBinary64Roundoff:"<100*2^-53",combinedAbsoluteError:"<2e-11",saturationError:"<2e-17",assumptions:"Lambert convergent bracketing; IEEE round-to-nearest normal operations; no unsafe fast-math. No native CPU or full-engine qualification."};
}
function denseCheck(candidate) {
  let maximumAbsoluteError=0, witness=0;
  for(let i=0;i<=400000;i++){
    const x=-20+i/10000, error=Math.abs(candidate(x)-Math.tanh(x));
    if(error>maximumAbsoluteError){maximumAbsoluteError=error;witness=x;}
  }
  if(!(maximumAbsoluteError<2e-11))throw new Error("dense numerical check");
  if(!Object.is(candidate(-0),-0)||candidate(Infinity)!==1||candidate(-Infinity)!==-1||!Number.isNaN(candidate(NaN)))throw new Error("special values");
  return {samples:400001,maximumAbsoluteError,witness,role:"empirical sanity check, not the uniform proof or a native performance result"};
}
console.log(JSON.stringify({certificate:runProof(),denseCheck:denseCheck(boundedTanhCandidate)},null,2));
