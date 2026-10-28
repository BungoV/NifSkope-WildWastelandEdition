# lane WEATHERBLEND1: cut the weather-change functions out of esmweather.cpp by text, for weatherblend.cpp.
# usage: python -I cut.py <esmweather.cpp for the helpers> <esmweather.cpp for the blends> <out cut.inc>
# The red passes the base commit's copy as the second file: its blends ignore the weather left behind.
import sys

HELPERS = ['void wwRgbToLab(', 'void wwLabToRgb(', 'void wwLabBlend(', 'void wwLabBlend4(',
           'double wwWeatherTransHours(', 'float wwWeatherTransPct(', 'double wwWeatherTransGameHours(',
           'void wwWeatherTransWeights(', 'void transSum(', 'float wwFogDayWeight(', 'void wwLutBlendN(',
           'quint32 wwRegionDraw(']
BLENDS = ['void EsmWeather::blendRow(', 'void EsmWeather::blendDalc(', 'void EsmWeather::blendRowLab(',
          'void EsmWeather::blendCloud(', 'float EsmWeather::blendSkyScale(', 'WwFog wwFogAt(']
# lane WEATHERBLEND2: the laws, cut with the blends; a base without one gets the viewer's old behaviour (the red)
LAWS = [('void wwImageSpaceSlots(',
         'void wwImageSpaceSlots(float tc,float tl,float pct,bool last,float w[4])\n{\n'
         '\tw[0]=w[1]=w[2]=w[3]=0.0f;\n'
         '\tif(last&&pct<0.5f){w[2]=1.0f-tl;w[3]=tl;}else{w[0]=1.0f-tc;w[1]=tc;}\n}\n'),
        ('float wwWeatherTransAccelPct(',
         'float wwWeatherTransAccelPct(float raw,float,const WwSkyGmst&)\n{\n\treturn raw;\n}\n'),
        ('int wwWeatherPickRule(',
         'int wwWeatherPickRule(bool,bool same,bool,bool)\n{\n\treturn same?WwPickKeep:WwPickStart;\n}\n'),
        # lane WEATHERBLEND3: the base's behaviour: no snap events, no wait, a scroll per weather, rounded wind, no offset
        ('int wwWeatherSnapRule(', 'int wwWeatherSnapRule(int rule,unsigned)\n{\n\treturn rule;\n}\n'),
        ('double wwWeatherWaitHours(', 'double wwWeatherWaitHours(double,double)\n{\n\treturn 0.0;\n}\n'),
        ('double wwWeatherTransPctSeconds(',
         'double wwWeatherTransPctSeconds(double,double,quint8,const WwSkyGmst&,float)\n{\n\treturn 0.0;\n}\n'),
        ('float wwCloudScroll(',
         'float wwCloudScroll(double,float,float v,double s,double)\n{\n'
         '\tconst double o=double(v)*0.1*s;\n\treturn float(o-std::floor(o));\n}\n'),
        ('float wwWeatherWindBlend(',
         'float wwWeatherWindBlend(quint8 c,quint8 l,float p)\n{\n'
         '\treturn float(std::lround(float(c)*p+float(l)*(1.0f-p)));\n}\n'),
        ('float wwWindRangeOffset(', 'float wwWindRangeOffset(quint8,quint8,quint32)\n{\n\treturn 0.0f;\n}\n'),
        # lane WEATHERBLEND4: the base's behaviour: the hour box forward only, the list's most likely weather, the first region
        ('double wwWeatherHourStep(',
         'double wwWeatherHourStep(double f,double t)\n{\n\tdouble d=std::fmod(t-f,24.0);\n\treturn d<0.0?d+24.0:d;\n}\n'),
        ('quint32 wwRegionWeatherPick(',
         'quint32 wwRegionWeatherPick(const WwRegionWeatherEntry*e,int n,quint32)\n{\n\tquint32 b=0;qint32 c=0;\n'
         '\tfor(int i=0;i<n;i++)if(e[i].chance>c){c=e[i].chance;b=e[i].weather;}\n\treturn b;\n}\n'),
        ('int wwRegionChoose(', 'int wwRegionChoose(const WwRegionRank*,int n)\n{\n\treturn n>0?0:-1;\n}\n'),
        # lane WEATHERBLEND5: the base's behaviour: no area = inside, nothing skipped, a listed weather kept outright
        ('bool wwRegionCounts(',
         'bool wwRegionCounts(const WwRegionWeather&g,quint32,int)\n{\n\treturn g.hasWeather&&(g.inside||!g.areas);\n}\n'),
        ('double wwWeatherDurationHours(', 'double wwWeatherDurationHours(int,float,const WwSkyGmst&)\n{\n\treturn 1e9;\n}\n'),
        ('double wwWeatherElapsedHours(', 'double wwWeatherElapsedHours(double,double)\n{\n\treturn 0.0;\n}\n'),
        ('quint32 wwRegionWeatherFollow(',
         'quint32 wwRegionWeatherFollow(const WwRegionWeatherEntry*e,int n,quint32 cur,bool,double,double,quint32 d,double)\n{\n'
         '\tfor(int i=0;i<n;i++)if(cur&&e[i].weather==cur)return cur;\n\treturn wwRegionWeatherPick(e,n,d);\n}\n'),
        # lane WEATHERBLEND6: the base's behavior: the spawn pick only, a border never re-picks
        ('bool wwRegionWalkDue(', 'bool wwRegionWalkDue(const WwRegionWalk&w,qint64,int,int,const float*)\n{\n\treturn !w.started;\n}\n'),
        ('bool wwRegionWalkSample(',
         'bool wwRegionWalkSample(WwRegionWalk&w,qint64,int,int,const float*,QVector<quint32>,quint32 c,int)\n{\n'
         '\tif(!w.started)w.region=c;\n\tw.started=true;\n\tw.samples++;\n\treturn false;\n}\n'),
        ('quint32 wwRegionWalkFollow(',
         'quint32 wwRegionWalkFollow(const WwRegionWeatherEntry*,int,quint32 cur,double,double,quint32,double,QString*)\n{\n'
         '\treturn cur;\n}\n')]


def cut(path, sig):
    lines = open(path, 'rb').read().decode('utf-8').replace('\r\n', '\n').split('\n')
    for i, l in enumerate(lines):
        if l.startswith(sig):
            for j in range(i + 1, len(lines)):
                if lines[j] == '}':
                    return '\n'.join(lines[i:j + 1]) + '\n'
    return None


def main():
    helpers, blends, out = sys.argv[1], sys.argv[2], sys.argv[3]
    text = ['// generated by cut.py from %s (helpers) and %s (blends)\n' % (helpers, blends)]
    for sig in HELPERS:
        body = cut(helpers, sig)
        if body is None:
            sys.exit('missing helper ' + sig)
        text.append(('namespace\n{\n' + body + '}\n') if sig == 'void transSum(' else body)
    for sig in BLENDS:
        body = cut(blends, sig)
        if body is None:
            sys.exit('missing blend ' + sig)
        text.append(body)
    for sig, stand in LAWS:
        body = cut(blends, sig)
        text.append(body if body is not None else '// stand-in: the base has no ' + sig + '\n' + stand)
    open(out, 'wb').write('\n'.join(text).encode('utf-8'))
    print('cut', len(HELPERS) + len(BLENDS), 'functions ->', out)


main()
