FROM fedora:42

# Install necessary packages
RUN dnf install -y \
    gcc \
    make \
    glibc-devel \
    glibc-headers \
    perf \
    procps-ng \
    && dnf clean all

# Set working directory
WORKDIR /app

# Add entrypoint
COPY entrypoint.sh .

# Perf options
ENV PERF_OPTS=""

# Thread count override
ENV THREADS=""

# Copy the C source code into the container
COPY thread_load_cpu_pin.c .
COPY thread_load_original.c .
COPY thread_load_optimized.c .

# Compile the program
RUN gcc -pthread -o thread_load_orig thread_load_original.c
RUN gcc -pthread -o thread_load_spin thread_load_optimized.c
RUN gcc -pthread -o thread_load_cpu_pin thread_load_cpu_pin.c

# Default command to run the test loop
ENTRYPOINT ["/app/entrypoint.sh"]
