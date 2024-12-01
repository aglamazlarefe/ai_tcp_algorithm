#ifndef RL_AQM_ENV_H
#define RL_AQM_ENV_H

#include "ns3/core-module.h"
#include "ns3/ai-module.h"

 
namespace ns3 {

/**
 *  \brief RLAqmEnv sınıfı, AQM için bir OpenGym ortamını tanımlar.
 */
class RLAqmEnv : public OpenGymEnv
{
public:
    static TypeId GetTypeId(void);
    RLAqmEnv();
    virtual ~RLAqmEnv() override;


    // OpenGymEnv arayüz metodları
    virtual Ptr<OpenGymSpace> GetActionSpace() override;
    virtual Ptr<OpenGymSpace> GetObservationSpace() override;
    virtual bool GetGameOver() override;
    virtual Ptr<OpenGymDataContainer> GetObservation() override;
    virtual float GetReward() override;
    virtual std::string GetExtraInfo() override;
    virtual bool ExecuteActions(Ptr<OpenGymDataContainer> action) override;

    // AQM'den durum bilgisi al
    void SetState(double queueDelay, double linkUtilization, double dropProbability);

private:
    double m_queueDelay;          // Kuyruk gecikmesi
    double m_linkUtilization;     // Bağlantı kullanımı
    double m_dropProbability;      // Paket düşürme olasılığı
    float m_reward;              // Ödül
};

} // namespace ns3

#endif /* RL_AQM_ENV_H */