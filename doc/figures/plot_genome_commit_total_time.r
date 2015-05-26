pdf("genome_total_time_commit.pdf")

data <- matrix(c( 322.7,0,42.25,207,12.67,21.9,10.45,9.13,8.83,4.625),ncol=5, nrow=2)

barplot(data, main="Total time with varying commit rates",
        ylab="Total time (s)", xlab="", col=c("darkblue","darkgreen"), 
        beside=TRUE,
        names.arg=c("10K", "100K", "1M", "4M", "none"), 
        legend=c("RVM", "BLCR"))
