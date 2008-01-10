# **********************************************************************
#
# Copyright (c) 2003-2007 ZeroC, Inc. All rights reserved.
#
# This copy of Ice is licensed to you under the terms described in the
# ICE_LICENSE file included in this distribution.
#
# **********************************************************************

top_srcdir	= ..\..\..

TARGETS		= client.exe helloservice.dll

C_SRCS		= Client.cs
S_SRCS		= HelloI.cs HelloServiceI.cs

GEN_SRCS	= $(GDIR)\Hello.cs

SDIR		= .

GDIR		= generated

!include $(top_srcdir)\config\Make.rules.mak.cs

MCSFLAGS	= $(MCSFLAGS) -target:exe

SLICE2CSFLAGS	= $(SLICE2CSFLAGS) --ice -I. -I$(slicedir)

client.exe: $(C_SRCS) $(GEN_SRCS)
	$(MCS) $(MCSFLAGS) -out:$@ -r:$(bindir)\icecs.dll $(C_SRCS) $(GEN_SRCS)

helloservice.dll: $(S_SRCS) $(GEN_SRCS)
	$(MCS) $(MCSFLAGS) -target:library -out:$@ -r:$(bindir)\iceboxcs.dll -r:$(bindir)\icecs.dll \
		$(S_SRCS) $(GEN_SRCS)

!include .depend