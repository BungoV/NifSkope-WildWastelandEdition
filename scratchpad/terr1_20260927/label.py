"""TERR1: put a 60 px title bar saying what the picture is above a full-size render (one picture per file).
usage: python label.py <in.png> <out.png> "<title>"
"""
import sys
from PIL import Image, ImageDraw, ImageFont

BAR = 60


def main():
    src, dst, title = sys.argv[1:4]
    im = Image.open(src).convert('RGB')
    out = Image.new('RGB', (im.width, im.height + BAR), (24, 24, 24))
    out.paste(im, (0, BAR))
    d = ImageDraw.Draw(out)
    try:
        font = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 30)
    except OSError:
        font = ImageFont.load_default()
    d.text((16, (BAR - 36) // 2), title, fill=(235, 235, 235), font=font)
    out.save(dst)
    print('labeled', dst, out.size)


if __name__ == '__main__':
    main()
