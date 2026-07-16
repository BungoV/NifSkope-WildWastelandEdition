// End-to-end C++ transfer, validated against Python (case.txt).
#include <cstdio>
#include <cmath>
#include <vector>
#include <array>
#include <map>
#include <algorithm>
#include <cstring>
using namespace std;

struct V3{double x,y,z;};
static double dot(const V3&a,const V3&b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static V3 sub(const V3&a,const V3&b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
static V3 add(const V3&a,const V3&b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
static V3 mul(const V3&a,double s){return {a.x*s,a.y*s,a.z*s};}

static V3 closestPtTri(const V3&p,const V3&a,const V3&b,const V3&c,double bary[3]){
    V3 ab=sub(b,a),ac=sub(c,a),ap=sub(p,a);
    double d1=dot(ab,ap),d2=dot(ac,ap);
    if(d1<=0&&d2<=0){bary[0]=1;bary[1]=0;bary[2]=0;return a;}
    V3 bp=sub(p,b);double d3=dot(ab,bp),d4=dot(ac,bp);
    if(d3>=0&&d4<=d3){bary[0]=0;bary[1]=1;bary[2]=0;return b;}
    double vc=d1*d4-d3*d2;
    if(vc<=0&&d1>=0&&d3<=0){double v=d1/(d1-d3);bary[0]=1-v;bary[1]=v;bary[2]=0;return add(a,mul(ab,v));}
    V3 cp=sub(p,c);double d5=dot(ab,cp),d6=dot(ac,cp);
    if(d6>=0&&d5<=d6){bary[0]=0;bary[1]=0;bary[2]=1;return c;}
    double vb=d5*d2-d1*d6;
    if(vb<=0&&d2>=0&&d6<=0){double w=d2/(d2-d6);bary[0]=1-w;bary[1]=0;bary[2]=w;return add(a,mul(ac,w));}
    double va=d3*d6-d5*d4;
    if(va<=0&&(d4-d3)>=0&&(d5-d6)>=0){double w=(d4-d3)/((d4-d3)+(d5-d6));bary[0]=0;bary[1]=1-w;bary[2]=w;return add(b,mul(sub(c,b),w));}
    double den=1.0/(va+vb+vc),v=vb*den,w=vc*den;bary[0]=1-v-w;bary[1]=v;bary[2]=w;return add(add(a,mul(ab,v)),mul(ac,w));
}

int main(){
    FILE*f=fopen("case.txt","r"); if(!f){printf("no case.txt\n");return 2;}
    char tag[32]; int dnv,dnt,nb;
    fscanf(f,"%s %d %d %d",tag,&dnv,&dnt,&nb);
    vector<V3> dp(dnv); for(auto&p:dp) fscanf(f,"%lf %lf %lf",&p.x,&p.y,&p.z);
    vector<int> tri(dnt*3); for(int i=0;i<dnt*3;i++) fscanf(f,"%d",&tri[i]);
    vector<array<int,4>> did(dnv); vector<array<double,4>> dw(dnv);
    for(int i=0;i<dnv;i++) for(int k=0;k<4;k++) fscanf(f,"%d %lf",&did[i][k],&dw[i][k]);
    int tnv; fscanf(f,"%s %d",tag,&tnv);
    vector<V3> tp(tnv); for(auto&p:tp) fscanf(f,"%lf %lf %lf",&p.x,&p.y,&p.z);
    fscanf(f,"%s",tag); // EXPECT
    vector<array<int,4>> eid(tnv); vector<array<double,4>> ew(tnv);
    for(int i=0;i<tnv;i++) for(int k=0;k<4;k++) fscanf(f,"%d %lf",&eid[i][k],&ew[i][k]);
    fclose(f);

    // incident triangles per donor vertex
    vector<vector<int>> inc(dnv);
    for(int t=0;t<dnt;t++) for(int j=0;j<3;j++) inc[tri[t*3+j]].push_back(t);

    double sumL1=0; int dom=0, cnt=0;
    for(int ti=0;ti<tnv;ti++){
        // nearest donor vertex
        int nv=0; double best=1e30;
        for(int i=0;i<dnv;i++){V3 d=sub(dp[i],tp[ti]);double dd=dot(d,d);if(dd<best){best=dd;nv=i;}}
        // refine over incident tris
        double bb=1e30, bbary[3]={1,0,0}; int bt=inc[nv].empty()?-1:inc[nv][0];
        for(int t:inc[nv]){int a=tri[t*3],b=tri[t*3+1],c=tri[t*3+2];double bary[3];
            V3 cp=closestPtTri(tp[ti],dp[a],dp[b],dp[c],bary);V3 d=sub(cp,tp[ti]);double dd=dot(d,d);
            if(dd<bb){bb=dd;bt=t;memcpy(bbary,bary,sizeof bbary);}}
        map<int,double> acc;
        int vv[3]={tri[bt*3],tri[bt*3+1],tri[bt*3+2]};
        for(int s=0;s<3;s++) for(int k=0;k<4;k++) if(did[vv[s]][k]>=0) acc[did[vv[s]][k]]+=bbary[s]*dw[vv[s]][k];
        // top-4 + normalize
        vector<pair<double,int>> v; for(auto&kv:acc) v.push_back({kv.second,kv.first});
        sort(v.rbegin(),v.rend()); if((int)v.size()>4) v.resize(4);
        double s=0; for(auto&pr:v) s+=pr.first; if(s<=0){cnt++;continue;}
        map<int,double> got; for(auto&pr:v) got[pr.second]=pr.first/s;
        // compare to expected
        map<int,double> exp; for(int k=0;k<4;k++) if(eid[ti][k]>=0) exp[eid[ti][k]]=ew[ti][k];
        double l1=0; for(auto&kv:got) l1+=fabs(kv.second-(exp.count(kv.first)?exp[kv.first]:0));
        for(auto&kv:exp) if(!got.count(kv.first)) l1+=kv.second;
        sumL1+=l1; cnt++;
        int gd=-1,ed=-1; double gm=-1,em=-1;
        for(auto&kv:got) if(kv.second>gm){gm=kv.second;gd=kv.first;}
        for(auto&kv:exp) if(kv.second>em){em=kv.second;ed=kv.first;}
        if(gd==ed) dom++;
    }
    printf("C++ transfer vs Python EXPECT: n=%d  meanL1=%.5f  dominant-match=%.1f%%\n",
           cnt, sumL1/cnt, 100.0*dom/cnt);
    printf("%s\n", (sumL1/cnt<0.001 && dom>0.99*cnt) ? "PASS (C++ reproduces Python)" : "DIVERGENCE");
    return 0;
}
