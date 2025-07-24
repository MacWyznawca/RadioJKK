
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void start_web_server(void);
void stop_web_server(void);

void JkkRadioWwwSetStationId(int8_t id);
void JkkRadioWwwUpdateStationList(const char *stationList);
void JkkRadioWwwUpdateEqList(const char *list);
void JkkRadioWwwUpdateVolume(uint8_t volume);
void JkkRadioWwwSetEqId(uint8_t id);
void JkkRadioWwwUpdateRecording(uint8_t rec);

#ifdef __cplusplus
}
#endif
