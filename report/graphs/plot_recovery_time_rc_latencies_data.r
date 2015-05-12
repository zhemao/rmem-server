input <- read.table("recovery_time_rc_latencies_data", header=TRUE)

options(scipen=5)
pdf("recovery_time_rc_latencies.pdf")

plot(input$x, input$y, main="Recovery latency Micro-Benchmark", xlab="Number of Pages", ylab="Latency (ms)")

lines(input$x, input$y)
