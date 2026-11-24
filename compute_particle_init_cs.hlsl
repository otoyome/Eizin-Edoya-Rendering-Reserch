#include "compute_particle.hlsli"

RWStructuredBuffer<particle_data> particle_data_buffer : register(u0); //  パーティクル管理バッファ
AppendStructuredBuffer<uint> particle_unused_buffer : register(u1); //  パーティクル番号管理バッファ(末尾への追加専用)

#if defined(UNIT_16_SUB_04)
RWStructuredBuffer<particle_header> particle_header_buffer : register(u3); //  パーティクルヘッダー管理バッファ
#endif  //  defined(UNIT_16_SUB_04)

[numthreads(NumParticleThread, 1, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    int index = dispatch_thread_id.x;

    //  パーティクル情報初期化
#if defined(UNIT_16_SUB_04)
    //  ヘッダーに位置が変わったのでここで初期化
    particle_header_buffer[index].alive = 0;
    particle_header_buffer[index].particle_index = 0;
#else   //  defined(UNIT_16_SUB_04)
    particle_data_buffer[index].parameter.z = -1.0f;
#endif  //  defined(UNIT_16_SUB_04)
    
    //  未使用リスト(AppendStructuredBuffer)の末尾に追加
    particle_unused_buffer.Append(index);
}
