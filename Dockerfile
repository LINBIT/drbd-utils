FROM centos:centos7 as builder

ENV DL_URL https://www.linbit.com/downloads/drbd/utils
# setup env, unfortunately ENV is too inflexible
ENV NV /tmp/env
COPY /configure.ac /tmp
RUN vers=$(grep "^AC_INIT" /tmp/configure.ac | cut -d'(' -f2 | cut -f2 -d ',' | sed 's/ //g') && test -n "$vers" && \
  echo "DRBD_UTILS_VERSION=$vers" > $NV && \
  echo "DRBD_UTILS_PKGNAME=drbd-utils" >> $NV && \
  echo 'DRBD_UTILS_TGZ=${DRBD_UTILS_PKGNAME}-${DRBD_UTILS_VERSION}.tar.gz' >> $NV && \
  echo "DRBD_UTILS_DL_TGZ=${DL_URL}"'/${DRBD_UTILS_TGZ}' >> $NV

USER root
RUN yum -y update-minimal --security --sec-severity=Important --sec-severity=Critical
RUN groupadd makepkg # !lbbuild
RUN useradd -m -g makepkg makepkg # !lbbuild

RUN yum install -y rpm-build wget gcc flex glibc-devel make automake && yum clean all -y # !lbbuild

RUN cd /tmp && . "$NV" && wget "$DRBD_UTILS_DL_TGZ" # !lbbuild
# =lbbuild COPY /${DRBD_UTILS_TGZ} /tmp/

USER makepkg

RUN cd ${HOME} && . "$NV" && \
  cp /tmp/${DRBD_UTILS_TGZ} ${HOME} && \
  mkdir -p ${HOME}/rpmbuild/SOURCES && \
  cp /tmp/${DRBD_UTILS_TGZ} ${HOME}/rpmbuild/SOURCES && \
  tar xvf ${DRBD_UTILS_TGZ} && \
  cd ${DRBD_UTILS_PKGNAME}-${DRBD_UTILS_VERSION} && \
  ./configure --with-prebuiltman && make drbd.spec && \
  rpmbuild -bb --without drbdmon --with prebuiltman --without sbinsymlinks --without manual --without heartbeat --without xen --without 83support --without 84support drbd.spec


FROM registry.access.redhat.com/ubi7/ubi
MAINTAINER Roland Kammerer <roland.kammerer@linbit.com>

ENV DRBD_UTILS_VERSION 9.11.0

ARG release=1
LABEL name="drbd-utils" \
      vendor="LINBIT" \
      version="$DRBD_UTILS_VERSION" \
      release="$release" \
      summary="low level userpace interacting with the DRBD kernel module" \
      description="low level userpace interacting with the DRBD kernel module"

COPY COPYING /licenses/gpl-2.0.txt

COPY --from=builder /home/makepkg/rpmbuild/RPMS/x86_64/drbd-utils*.rpm /tmp/
RUN yum -y update-minimal --security --sec-severity=Important --sec-severity=Critical && \
  yum install -y /tmp/drbd-utils*.rpm && yum clean all -y
RUN echo 'global { usage-count no; }' > /etc/drbd.d/global_common.conf
