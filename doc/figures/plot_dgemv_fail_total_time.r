pdf("dgemv_total_time_fail.pdf")

data <- matrix(c(185.14,122.14,50.42,45.14,39.76,34.21,29.03,30.63,23.96,25.98,22.75,24.98),ncol=6, nrow=2)

barplot(data, main="Total time with varying fail rates",
        ylab="Total time (s)", xlab="", col=c("darkblue","darkgreen"), 
        beside=TRUE,
        names.arg=c("1", "5", "10", "20", "50", "100"), 
        legend=c("RVM", "File"))

