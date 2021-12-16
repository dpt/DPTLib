DPTLib bmfont
=============

bmfont is a bitmap font engine which can draw proportional fonts. It reads
font definitions from PNG files which have the glyphs laid out in a grid with
extra lines inserted that define the advance widths.

It can handle 4bpp and 32bpp pixels at the time of writing.

Its rendering should be reasonably quick.

It supports both opaque and transparent backgrounds.


PNG format
----------
Should be 32 characters wide. bmfont works out the width and height from its dimensions.

It should be a 4 colour PNG (an 8bpp 4-palette-entry image wouldn't do).


Measuring
---------


Rendering
---------


Limits
------
No character mapping yet.
No kerning.

