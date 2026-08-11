# Pixel comparison for the render gates.
#
# Prints one space-separated line:
#   <differingPx> <maxChannelDelta> <mean|delta|> <fractionOfFrame> <x0> <y0> <x1> <y1>
#   <leftPx> <rightPx> <WxH> <centroidX> <centroidY>
# or "ERR size-mismatch". Kept apart from the drivers so the bash gates and
# capture.ps1 measure with the same code.
#
# The centroid is weighted by |delta|, so it answers "where on screen is this
# change happening" rather than "where did any bit flip" - which is what a
# spatial gate (does vertex alpha suppress the effect on the half it is zero on)
# actually needs.

param([Parameter(Mandatory=$true)][string]$A,[Parameter(Mandatory=$true)][string]$B)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;using System.Drawing;using System.Drawing.Imaging;using System.Runtime.InteropServices;
public class WwImgDiff {
  public static string Compare(string a, string b) {
    using (var ia = new Bitmap(a)) using (var ib = new Bitmap(b)) {
      if (ia.Width != ib.Width || ia.Height != ib.Height) return "ERR size-mismatch";
      var r = new Rectangle(0,0,ia.Width,ia.Height);
      var da = ia.LockBits(r, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
      var db = ib.LockBits(r, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
      long n = ia.Width * (long)ia.Height * 4;
      byte[] ba = new byte[n], bb = new byte[n];
      Marshal.Copy(da.Scan0, ba, 0, (int)n); Marshal.Copy(db.Scan0, bb, 0, (int)n);
      ia.UnlockBits(da); ib.UnlockBits(db);
      long diff=0, sum=0, left=0, right=0;
      double wx=0, wy=0;
      int minx=int.MaxValue,miny=int.MaxValue,maxx=-1,maxy=-1,mx=0;
      for (long i=0;i<n;i+=4) {
        int d = Math.Abs(ba[i]-bb[i])+Math.Abs(ba[i+1]-bb[i+1])+Math.Abs(ba[i+2]-bb[i+2]);
        if (d==0) continue;
        diff++; sum+=d;
        int per=Math.Max(Math.Abs(ba[i]-bb[i]),Math.Max(Math.Abs(ba[i+1]-bb[i+1]),Math.Abs(ba[i+2]-bb[i+2])));
        if(per>mx)mx=per;
        int px=(int)((i/4)%ia.Width), py=(int)((i/4)/ia.Width);
        if(px<minx)minx=px; if(px>maxx)maxx=px;
        if(py<miny)miny=py; if(py>maxy)maxy=py;
        if(px < ia.Width/2) left++; else right++;
        wx += px*(double)d; wy += py*(double)d;
      }
      double meanD = diff>0 ? (double)sum/diff : 0.0;
      double frac  = (double)diff/(ia.Width*(double)ia.Height);
      double cx = sum>0 ? wx/sum : 0.0, cy = sum>0 ? wy/sum : 0.0;
      return string.Format(System.Globalization.CultureInfo.InvariantCulture,
        "{0} {1} {2:F3} {3:F5} {4} {5} {6} {7} {8} {9} {10}x{11} {12:F1} {13:F1}",
        diff, mx, meanD, frac,
        (minx==int.MaxValue?0:minx), (miny==int.MaxValue?0:miny), maxx, maxy,
        left, right, ia.Width, ia.Height, cx, cy);
    }
  }
}
'@ -ReferencedAssemblies System.Drawing

[WwImgDiff]::Compare($A,$B)
