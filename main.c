#include <stdio.h>
#include "xensiveco2.h"

int main()
{
    printf("CO2 example applciation\n");
    Error_t ret;
    uint16_t co2ppm;

    ret = begin(true,false);
    if(ret != XENSIV_PASCO2_OK)
    {
        printf("CO2 intialization error\n");
        goto exitapp;
    }

    ret = setPressRef(900);
    if(ret != XENSIV_PASCO2_OK)
    {
        printf("CO2 Reference error\n");
        goto exitapp;
    }

    ret = startMeasure(10, 0, NULL);
    if(ret != XENSIV_PASCO2_OK)
    {
        printf("CO2 measure error\n");
        goto exitapp;
    }

    while(1)
    {
        ret = getCO2(&co2ppm);
        if(ret == XENSIV_PASCO2_OK)
        {
            printf("CO2 PPM: %d\n", co2ppm);
        }
        else
        {
            printf("CO2 read error\n");
            break;
        }
    }

exitapp:
    end(true,false);
    return 0;
}
