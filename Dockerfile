FROM alpine:latest

RUN apk add --no-cache doxygen lighttpd

WORKDIR /app

COPY . /app

RUN doxygen Doxyfile || true

RUN mkdir -p /var/www/localhost/htdocs && \
    cp -r docs/html/* /var/www/localhost/htdocs/ || true

EXPOSE 24354

CMD ["lighttpd", "-D", "-f", "/etc/lighttpd/lighttpd.conf"]
