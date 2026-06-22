./server &
pid=$!

for i in {1..100}; do
  echo "hello" | nc 127.0.0.1 8080
done

lsof -p $pid