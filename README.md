Compile with:
gcc ./src/*.c ./src/*.h -o jitter

Run with:
jitter -c <target cpu> -d <duration seconds> -r <reporting interval> -o <output configuration>

Output is driven by -o argument. Currently there are 2 types of output available:
-o influx://localhost:4321 - will submit results to influxDb using line protocol over UDP
-o csv:///tmp/jitter.csv - will store results in /tmp/jitter.csv