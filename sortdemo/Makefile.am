bin_PROGRAMS = sortdemo
sortdemo_SOURCES = sortdemo.cpp mainfrm.cpp mainview.cpp sortproc.cpp stdafx.cpp SortdemoResource.cpp sortdemo.h mainfrm.h mainview.h sortproc.h stdafx.h
man1_MANS = @MAN_PAGES@
RES_DIST = res/sortdemo.ico res/toolbar1.bmp res/toolbar10.bmp res/toolbar11.bmp res/toolbar2.bmp res/toolbar3.bmp res/toolbar4.bmp res/toolbar5.bmp res/toolbar6.bmp res/toolbar7.bmp res/toolbar8.bmp res/toolbar9.bmp
EXTRA_DIST = SortdemoResource.xrc sortdemo.1 sortdemo.interix Makefile.w32 $(RES_DIST) $(man1_MANS) 

./SortdemoResource.cpp : ./SortdemoResource.xrc
	@if ( test "$WXRC_PATH" = "" ) ; then
	  @echo "you must manually use 'wxrc' to build 'SortdemoResource.cpp' from "
	  @echo "'SortdemoResource.xrc'.  If this poses a problem, use the 'touch'"
	  @echo "utility on 'SortdemoResource.cpp' to make its date newer than the"
	  @echo "XML resource file 'SortdemoResource.xrc', and retry 'make'."
	  @exit 1 ;
	@fi
	$(WXRC_PATH) --cpp-code --output ./SortdemoResource.cpp ./SortdemoResource.xrc


