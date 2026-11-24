#include "compute_particle.hlsli"

#if defined(UNIT_16_SUB_03)
RWStructuredBuffer<particle_data> particle_data_buffer : register(u0);
RWByteAddressBuffer indirect_data_buffer : register(u2);

#if defined(UNIT_16_SUB_04)
RWStructuredBuffer<particle_header> particle_header_buffer : register(u3);
#endif  //  defined(UNIT_16_SUB_04)

[numthreads(1, 1, 1)]
void main()
{
    //  パーティクル総数を取得
    uint current_num_particle = indirect_data_buffer.Load(IndirectArgumentsNumCurrentParticle);
    if (current_num_particle == 0)
        return;

    //  簡易的にソートを行う
    //  クイックソートなどの再帰関数を使うものはコンピュートシェーダーの方で制限がかかっているため無理。
#if 01  //  挿入ソート
    //  マシになったけどやっぱり遅い
    for (int h = current_num_particle / 2; h > 0; h /= 2)
    {
        for (int i = h; i < current_num_particle; i += 1)
        {
#if defined(UNIT_16_SUB_04)
            particle_header k = particle_header_buffer[i];

            int j;
            for (j = i; j >= h && (particle_header_buffer[j - h].alive < k.alive); j -= h)
            {
                particle_header_buffer[j] = particle_header_buffer[j - h];
            }

            particle_header_buffer[j] = k;
#else  //  defined(UNIT_16_SUB_04)
            particle_data k = particle_data_buffer[i];

            int j;
            for (j = i; j >= h && (particle_data_buffer[j - h].parameter.z < k.parameter.z); j -= h)
            {
                particle_data_buffer[j] = particle_data_buffer[j - h];
            }

            particle_data_buffer[j] = k;
#endif  //  defined(UNIT_16_SUB_04)
        }
    }
#else   //  バブルソートparticle_data_bufferでのみやらせる
    //  バブルソートは無理がある事を伝えるため意図的にやらせる
    for (int i = 0; i != (int) current_num_particle; ++i)
    {
        for (int j = current_num_particle - 1; j > i; --j)
        {
            //  生存フラグだけ見て処理する
#if defined(UNIT_16_SUB_04)
            if (particle_header_buffer[j].alive < particle_header_buffer[j - 1].alive)
            {
                particle_header temp = particle_header_buffer[j - 1];
                particle_header_buffer[j - 1] = particle_header_buffer[j];
                particle_header_buffer[j] = temp;
            }
#else  //  defined(UNIT_16_SUB_04)
            if (particle_data_buffer[j].parameter.z < particle_data_buffer[j - 1].parameter.z)
            {
                particle_data temp = particle_data_buffer[j - 1];
                particle_data_buffer[j - 1] = particle_data_buffer[j];
                particle_data_buffer[j] = temp;
            }
#endif  //  defined(UNIT_16_SUB_04)
        }
    }
#endif
}
#else	//	defined(UNIT_16_SUB_03)
[numthreads(1, 1, 1)]
void main()
{
}
#endif	//	defined(UNIT_16_SUB_03)
