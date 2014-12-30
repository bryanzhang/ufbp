#include <stdio.h>
#include <sys/time.h>

int main()
{
    struct timeval localTime;
    gettimeofday(&localTime, NULL); //Time zone struct is obsolete, hence NULL

    printf("The curr time in seconds %ld\n", localTime.tv_sec);
    printf("The curr time in microseconds %ld\n", localTime.tv_usec);

    return 0;
}
