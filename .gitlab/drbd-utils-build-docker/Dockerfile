FROM ubuntu:bionic

RUN apt-get update && apt-get install -y \
	    git make gawk autoconf flex \
	    clitest \
	    jq parallel xsltproc \
	    docbook-xsl po4a pkg-config && \
	    rm -rf /var/lib/apt/lists/*
