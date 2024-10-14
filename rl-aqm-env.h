#ifndef RL_AQM_ENV_H
#define RL_AQM_ENV_H

#include "ns3/open-gym-env.h"

namespace ns3 {

/**
 * \brief RL-AQM için OpenGymEnv sınıfı.
 */
class RLAqmEnv : public OpenGymEnv
{
public:
    RLAqmEnv();
    ~RLAqmEnv() override;

    static TypeId GetTypeId(void);

    Ptr<OpenGymSpace> GetActionSpace() override;
    Ptr<OpenGymSpace> GetObservationSpace() override;

    bool GetGameOver() override;

    Ptr<OpenGymDataContainer> GetObservation() override;
    float GetReward() override;
    std::string GetExtraInfo() override;

    bool ExecuteActions(Ptr<OpenGymDataContainer> action) override;

    // AQM'den durum bilgisi al
    void SetState(double queueDelay, double linkUtilization, double dropProbability);

private:
    double m_queueDelay;              // Kuyruk gecikmesi
    double m_linkUtilization;       // Bağlantı kullanımı
    double m_dropProbability;         // Paket bırakma olasılığı
    float m_reward;                  // Ödül
};

} // namespace ns3

#endif /* RL_AQM_ENV_H */