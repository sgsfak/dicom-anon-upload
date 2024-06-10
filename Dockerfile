FROM busybox:1.36.1-uclibc as busybox

FROM ubuntu/jre:17-22.04_44

# copy the static shell utils into base image
COPY --from=busybox /bin/sh /bin/sh
COPY --from=busybox /bin/env /bin/env
COPY --from=busybox /bin/echo /bin/echo
COPY --from=busybox /bin/ls /bin/ls

COPY ./ctp /app
COPY ./run_docker.sh /app

VOLUME /input /output

ENTRYPOINT [ ]
CMD [ "/app/run_docker.sh" ]
