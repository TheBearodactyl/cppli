FROM alpine:latest
RUN apk add --no-cache doxygen python3 py3-pip
WORKDIR /app
COPY . /app
RUN doxygen Doxyfile || true
RUN mkdir -p /app/docs/html
EXPOSE 24354
CMD ["python3", "-m", "http.server", "24354", "--directory", "/app/docs/html"]
