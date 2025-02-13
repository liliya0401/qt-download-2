FROM quay.io/homdx/qt-android-docker:513

#ARG ADBCACHE_VERSION=1.0.6
#ARG ADBCACHE_HASH=f64ce7ec93b7dc78b0b3a0227a369604221c9bd2d54c33d1d3e1865d720a7351cc91a99008a4ea66ab799c8e76688f442bbc65c30255baae9497e9c67c6c5f0e
#ARG ADBCACHEFILE=android.tar.gz

#RUN cd /root && set -ex && curl -s -L -o android.tar.gz https://github.com/homdx/qt-view/releases/download/${ADBCACHE_VERSION}/${ADBCACHEFILE} \
#    && echo "${ADBCACHE_HASH}  android.tar.gz" | sha512sum -c \
#    && tar -xvf android.tar.gz \
#    && ls -la android.tar.gz && cd ..

ARG projname=qt-download-2
ARG profile=qt-download-2.pro

RUN cd / && git clone https://github.com/homdx/android_openssl.git && cd /android_openssl && echo git checkout 5.12.4_5.13.0 && mkdir -pv /app/${projname}

ADD . /app/${projname}/

WORKDIR /app/${projname}/src

RUN export ANDROID_TARGET_SDK_VERSION=28 && \
    echo run && cp -vf ${profile}.buildapk ${profile} && \
    mkdir -pv android-build/libs/armeabi-v7a/ && \
    echo 'fix missing libc++_shared.so' && cp -vf /android-ndk-r19c/sources/cxx-stl/llvm-libc++/libs/armeabi-v7a/libc++_shared.so android-build/libs/armeabi-v7a/ && \
    echo update translate files && lrelease ${profile} && \
    build-android-gradle-project ${profile} --debug && \
    echo copy result apk && \
    cp -vf android-build/build/outputs/apk/debug/android-build-debug.apk /app || echo error build

CMD tail -f /var/log/faillog
