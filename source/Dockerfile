FROM ubuntu:latest AS build

RUN apt-get update \
    && apt-get install -y gcc make

WORKDIR /var/build

COPY . ./

RUN make clean && make static

FROM scratch

COPY --from=build /var/build/server /server

CMD ["/server", "8000"]