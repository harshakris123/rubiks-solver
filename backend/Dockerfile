# Build context must be the repo root (so this stage can see ../solver).
FROM gcc:13-bookworm AS solver-build
WORKDIR /src/solver
COPY solver/ .
RUN mkdir -p build && \
    g++ -std=c++17 -O2 -o build/solver_cli main.cpp Cube.cpp Move.cpp Solver.cpp

FROM python:3.11-slim
WORKDIR /app

COPY backend/requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY backend/ .
COPY --from=solver-build /src/solver/build/solver_cli /app/solver_cli

ENV SOLVER_BINARY_PATH=/app/solver_cli
ENV PORT=8000
EXPOSE 8000

CMD ["sh", "-c", "uvicorn main:app --host 0.0.0.0 --port ${PORT}"]
