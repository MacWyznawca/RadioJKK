
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void start_web_server(void);
void stop_web_server(void);

void JkkRadioWwwSetStationId( int8_t id);
void JkkRadioWwwUpdateStationList(const char *stationList);
void JkkRadioWwwUpdateVolume(uint8_t volume);

#ifdef __cplusplus
}
#endif
