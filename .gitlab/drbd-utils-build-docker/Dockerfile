FROM ubuntu:bionic

RUN apt-get update && apt-get install -y \
	    devscripts git build-essential make \
	    autoconf flex jq parallel xsltproc \
	    docbook-xsl po4a pkg-config && \
	    rm -rf /var/lib/apt/lists/*
