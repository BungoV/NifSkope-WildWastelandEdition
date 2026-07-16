// Standalone verification of the Rigging skin-core math ported to C++.
// Mirrors the validated Python (transfer.py / invbind.py). Portable structs here
// map to NifSkope Vector3 / Matrix4 in the real integration.
#include <cstdio>
#include <cmath>
#include <algorithm>

struct Vec3 { double x,y,z;
    Vec3(double a=0,double b=0,double c=0):x(a),y(b),z(c){}
    Vec3 operator-(const Vec3&o)const{return Vec3(x-o.x,y-o.y,z-o.z);}
    Vec3 operator+(const Vec3&o)const{return Vec3(x+o.x,y+o.y,z+o.z);}
    Vec3 operator*(double s)const{return Vec3(x*s,y*s,z*s);}
};
static double dot(const Vec3&a,const Vec3&b){return a.x*b.x+a.y*b.y+a.z*b.z;}

// Ericson closest-point-on-triangle, returns closest point + barycentric (u,v,w)
static Vec3 closestPtTri(const Vec3&p,const Vec3&a,const Vec3&b,const Vec3&c,double bary[3]){
    Vec3 ab=b-a, ac=c-a, ap=p-a;
    double d1=dot(ab,ap), d2=dot(ac,ap);
    if(d1<=0&&d2<=0){bary[0]=1;bary[1]=0;bary[2]=0;return a;}
    Vec3 bp=p-b; double d3=dot(ab,bp), d4=dot(ac,bp);
    if(d3>=0&&d4<=d3){bary[0]=0;bary[1]=1;bary[2]=0;return b;}
    double vc=d1*d4-d3*d2;
    if(vc<=0&&d1>=0&&d3<=0){double v=d1/(d1-d3);bary[0]=1-v;bary[1]=v;bary[2]=0;return a+ab*v;}
    Vec3 cp=p-c; double d5=dot(ab,cp), d6=dot(ac,cp);
    if(d6>=0&&d5<=d6){bary[0]=0;bary[1]=0;bary[2]=1;return c;}
    double vb=d5*d2-d1*d6;
    if(vb<=0&&d2>=0&&d6<=0){double w=d2/(d2-d6);bary[0]=1-w;bary[1]=0;bary[2]=w;return a+ac*w;}
    double va=d3*d6-d5*d4;
    if(va<=0&&(d4-d3)>=0&&(d5-d6)>=0){double w=(d4-d3)/((d4-d3)+(d5-d6));bary[0]=0;bary[1]=1-w;bary[2]=w;return b+(c-b)*w;}
    double den=1.0/(va+vb+vc), v=vb*den, w=vc*den;
    bary[0]=1-v-w;bary[1]=v;bary[2]=w;return a+ab*v+ac*w;
}

// row-major 4x4
static void mul4(const double*A,const double*B,double*C){
    for(int r=0;r<4;r++)for(int c=0;c<4;c++){double s=0;for(int k=0;k<4;k++)s+=A[r*4+k]*B[k*4+c];C[r*4+c]=s;}
}
// general 4x4 inverse via Gauss-Jordan
static bool inv4(const double*m,double*out){
    double a[4][8];
    for(int r=0;r<4;r++){for(int c=0;c<4;c++){a[r][c]=m[r*4+c];a[r][c+4]=(r==c)?1.0:0.0;}}
    for(int col=0;col<4;col++){
        int piv=col; for(int r=col+1;r<4;r++) if(std::fabs(a[r][col])>std::fabs(a[piv][col])) piv=r;
        if(std::fabs(a[piv][col])<1e-12) return false;
        for(int c=0;c<8;c++) std::swap(a[col][c],a[piv][c]);
        double d=a[col][col]; for(int c=0;c<8;c++) a[col][c]/=d;
        for(int r=0;r<4;r++) if(r!=col){double f=a[r][col];for(int c=0;c<8;c++) a[r][c]-=f*a[col][c];}
    }
    for(int r=0;r<4;r++)for(int c=0;c<4;c++) out[r*4+c]=a[r][c+4];
    return true;
}

static double maxdiff(const double*a,const double*b,int n){double m=0;for(int i=0;i<n;i++)m=std::max(m,std::fabs(a[i]-b[i]));return m;}

int main(){
    int fails=0;
    // TEST 1: closest point on triangle (inside projection)
    {Vec3 a(0,0,0),b(1,0,0),c(0,1,0),p(0.3,0.2,0.7);double bary[3];
     Vec3 cp=closestPtTri(p,a,b,c,bary);
     double ecp[3]={0.3,0.2,0.0}, eb[3]={0.5,0.3,0.2};
     double gc[3]={cp.x,cp.y,cp.z};
     double e=std::max(maxdiff(gc,ecp,3),maxdiff(bary,eb,3));
     printf("T1 closestPtTri(inside)   maxerr=%.2g %s\n",e,e<1e-6?"PASS":"FAIL"); if(e>=1e-6)fails++;}
    // TEST 2: closest point (outside -> edge)
    {Vec3 a(0,0,0),b(1,0,0),c(0,1,0),p(2,2,0.5);double bary[3];
     Vec3 cp=closestPtTri(p,a,b,c,bary);
     double ecp[3]={0.5,0.5,0.0}, eb[3]={0.0,0.5,0.5};
     double gc[3]={cp.x,cp.y,cp.z};
     double e=std::max(maxdiff(gc,ecp,3),maxdiff(bary,eb,3));
     printf("T2 closestPtTri(edge)     maxerr=%.2g %s\n",e,e<1e-6?"PASS":"FAIL"); if(e>=1e-6)fails++;}
    // TEST 3: inverse-bind  skinToBone = inv(boneWorld) * meshBind
    {double BW[16]={0.0,0.0,-1.0,-0.000167, 0.064981,0.997887,0.0,-0.881786, 0.997886,-0.064981,0.0,120.843651, 0,0,0,1};
     double MB[16]={1.0,-0.0,0.0,-0.000199, 0.0,1.0,-0.0,-0.881803, -0.0,0.0,1.0,120.843682, 0,0,0,1};
     double EXP[16]={0.0,0.064981,0.997887,3e-05, 0.0,0.997887,-0.064981,-1.9e-05, -1.0,0.0,0.0,3.2e-05, 0,0,0,1};
     double invBW[16],S2B[16]; inv4(BW,invBW); mul4(invBW,MB,S2B);
     double e=maxdiff(S2B,EXP,16);
     printf("T3 inverseBind (HEAD)     maxerr=%.2g %s\n",e,e<5e-5?"PASS":"FAIL"); if(e>=5e-5)fails++;}
    printf("\n%s (%d failures)\n", fails==0?"ALL PASS":"FAILURES", fails);
    return fails;
}
