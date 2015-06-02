pdf("dgemv_total_time_commit.pdf")

data <- matrix(c(21.08,25.87,19.23,24.53,19.2,24.27,19.04,24.23,18.74,24.8,18.87,24.64,18.8,24.63),ncol=7, nrow=2)

barplot(data, main="Total time with varying commit rates",
        ylab="Total time (s)", xlab="", col=c("darkblue","darkgreen"), 
        beside=TRUE,
        names.arg=c("1", "5", "10", "20", "50", "100", "none"), 
        legend=c("RVM", "File"))

