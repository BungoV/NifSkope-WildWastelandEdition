import numpy as np
def fex(mu,n):
    s2=1-mu*mu; t=n*n-s2
    g=np.sqrt(np.maximum(t,0))
    rs=((mu-g)/(mu+g))**2; rp=((n*n*mu-g)/(n*n*mu+g))**2
    return np.where(t<0,1.0,0.5*(rs+rp))
mu=np.linspace(0,1,200001)[1:]
def favg(n): return np.trapz(2*mu*fex(mu,n),mu)
def kc(n): return (n-1)/(4.08567+1.00071*n)
for n in [1.0,1.05,1.1,1.2,1.329,1.4,1.5,2.0,3.0]:
    F0=((n-1)/(n+1))**2; fa=favg(n); f90=F0+21*(fa-F0); f90k=F0+21*(kc(n)-F0)
    print(f"n={n} F0={F0:.4f} Favg={fa:.4f} kc={kc(n):.4f} F90={f90:.3f} F90kc={f90k:.3f}")
for deg in [80,85]:
    m=np.cos(np.radians(deg)); p=(1-m)**5
    print(deg,"exact1.5",fex(np.array([m]),1.5)[0],"exact1.329",fex(np.array([m]),1.329)[0],"schlick.04",.04+.96*p,"schlick.02",.02+.98*p)
