input <- read.table("commit_time_rc_latencies_data", header=TRUE)

options(scipen=5)
pdf("commit_time_rc_latencies.pdf")

plot(input$x, input$y, main="Commit latency Micro-Benchmark", xlab="Number of Pages", ylab="Latency (ms)")

lines(input$x, input$y)
