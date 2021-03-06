// This is brl/bbas/bsta/bsta_sampler.txx
#ifndef bsta_sampler_txx_
#define bsta_sampler_txx_
//:
// \file

#include "bsta_sampler.h"
#include <vnl/vnl_random.h>
#include <vcl_cstdlib.h>
#include <vcl_set.h>
#include <vcl_utility.h>
#include <vcl_algorithm.h>

//: For sorting pairs by their first element

template<class T>
class first_less
{
 public:
  bool
  operator()( const vcl_pair<float,T>& left_pair,
              const vcl_pair<float,T>& right_pair ) const
  {
    return left_pair.first < right_pair.first;
  }
};

//: wrapper around version with user-provided random generator
template <class T>
bool bsta_sampler<T>::sample(vcl_vector<T>& samples, vcl_vector<float>& p,
                             unsigned cnt, vcl_vector<T>& out)
{
  vnl_random rand;
  return sample(samples,p,cnt,out,rand);
}


//: put cnt samples into output vector wrt given probabilities
//  The sum of probabilities should sum to (approx.) 1; otherwise return false
template <class T>
bool bsta_sampler<T>::sample(vcl_vector<T>& samples, vcl_vector<float>& p,
                             unsigned cnt, vcl_vector<T>& out, vnl_random &rng) {
  if (p.size() != samples.size())
    return false;
  if (!p.size())
    return false;

  vcl_set<vcl_pair<float, unsigned>, first_less<unsigned> > accum_p;
  accum_p.insert(vcl_pair<float, unsigned>(p[0], 0));  // put the id of the sample

  for (unsigned i = 1; i < p.size(); i++) {
    float accum = (*(accum_p.rbegin())).first + p[i];
    accum_p.insert(vcl_pair<float, unsigned>(accum, i));
  }
  float last_val = (*(accum_p.rbegin())).first;
  unsigned last_id = (*(accum_p.rbegin())).second;
  if (last_val > 1.015625f || last_val < 0.984375f)
    return false;

  for (unsigned i = 0; i < cnt; i++) {
    float r = (float)(rng.drand32());
    if (r >= last_val) {
      out.push_back(samples[last_id]);
    } else {
      vcl_pair<float, unsigned> search_key(r, 0);
      unsigned r_id = (*(accum_p.upper_bound(search_key))).second;
      out.push_back(samples[r_id]);
    }
  }

  return true;
}

//: sample from a joint histogram treating it as a discrete prob distribution
template <class T>
bool bsta_sampler<T>::sample(const bsta_joint_histogram<float>& jh, unsigned cnt, vcl_vector<vcl_pair<float, float> >& out)
{
  vnl_random rng;
  return sample(jh, cnt, out, rng);
}

//: sample from a joint histogram treating it as a discrete prob distribution
// User provided vnl_random
template <class T>
bool bsta_sampler<T>::sample(const bsta_joint_histogram<float>& jh, unsigned cnt, vcl_vector<vcl_pair<float, float> >& out, vnl_random &rng)
{
  unsigned int na = jh.nbins_a();
  unsigned int nb = jh.nbins_b();

  vcl_set<vcl_pair<float, vcl_pair<unsigned, unsigned> >, first_less<vcl_pair<unsigned, unsigned> > > accum_p;
  vcl_pair<unsigned, unsigned> pi(0,0);
  float p_0 = jh.p((unsigned)0,(unsigned)0);
  accum_p.insert(vcl_pair<float, vcl_pair<unsigned, unsigned> >(p_0, pi));  // put the id of the sample

  // first scan the first row
  for (unsigned ib = 1; ib < nb; ib++) {
    vcl_pair<unsigned, unsigned> pi(0,ib);

    float accum = (*(accum_p.rbegin())).first + jh.p(0, ib);
    accum_p.insert(vcl_pair<float, vcl_pair<unsigned, unsigned> >(accum, pi));
  }

  // now do the rest of the rows
  for (unsigned ia = 1; ia < na; ia++)
    for (unsigned ib = 0; ib < nb; ib++) {
      vcl_pair<unsigned, unsigned> pi(ia,ib);

      float accum = (*(accum_p.rbegin())).first + jh.p(ia, ib);
      accum_p.insert(vcl_pair<float, vcl_pair<unsigned, unsigned> >(accum, pi));
    }

  float last_val = (*(accum_p.rbegin())).first;
  //unsigned last_id = (*(accum_p.rbegin())).second;

  if (last_val > 1.015625f || last_val < 0.984375f)
    return false;

  for (unsigned i = 0; i < cnt; i++) {
    float r = (float)(rng.drand32());
    if (r >= last_val) {
      // push the last sample in the histogram
      vcl_pair<float, float> out_sample(jh.min_a() + na*jh.delta_a(), jh.min_b() + nb*jh.delta_b());
      out.push_back(out_sample);
    } else {
      vcl_pair<float, vcl_pair<unsigned, unsigned> > search_key(r, vcl_pair<unsigned, unsigned>(0, 0));
      vcl_pair<unsigned, unsigned> r_id = (*(accum_p.upper_bound(search_key))).second;
      vcl_pair<float, float> out_sample(jh.min_a() + (r_id.first+1)*jh.delta_a(), jh.min_b() + (r_id.second+1)*jh.delta_b());
      out.push_back(out_sample);
    }
  }

  return true;
}

bool first_greater(const vcl_pair<float,vcl_pair<unsigned, unsigned> >& left_pair,
                   const vcl_pair<float,vcl_pair<unsigned, unsigned> >& right_pair )
{
  return left_pair.first > right_pair.first;
}


//: sample in the decreasing order of likelihood (i.e. the most likely bin will be returned as the first sample)
template <class T>
bool bsta_sampler<T>::sample_in_likelihood_order(const bsta_joint_histogram<float>& jh, unsigned cnt, vcl_vector<vcl_pair<float, float> >& out)
{
  unsigned int na = jh.nbins_a();
  unsigned int nb = jh.nbins_b();

  vcl_vector<vcl_pair<float, vcl_pair<unsigned, unsigned> > > arr;
  for (unsigned ia = 0; ia < na; ia++) {
    for (unsigned ib = 0; ib < nb; ib++) {
      vcl_pair<unsigned, unsigned> ind(ia, ib);
      vcl_pair<float, vcl_pair<unsigned, unsigned> > val(jh.p(ia, ib), ind);
      arr.push_back(val);
    }
  }

  vcl_sort(arr.begin(), arr.end(), first_greater);
  unsigned size = arr.size() < cnt ? arr.size() : cnt;
  for (unsigned i = 0; i < size; i++) {
    //vcl_cout << "sampled from bina: " << arr[i].second.first << " binb: " << arr[i].second.second << "!\n";
    vcl_pair<float, float> out_p(jh.min_a() + (arr[i].second.first+1)*jh.delta_a(), jh.min_b() + (arr[i].second.second+1)*jh.delta_b());
    out.push_back(out_p);
  }

  return true;
}

//: sample in the decreasing order of likelihood (i.e. the most likely bin will be returned as the first sample)
template <class T>
bool bsta_sampler<T>::sample_in_likelihood_order(const bsta_joint_histogram<float>& jh, unsigned cnt, vcl_vector<vcl_pair<unsigned, unsigned> >& out_indices)
{
  unsigned int na = jh.nbins_a();
  unsigned int nb = jh.nbins_b();

  vcl_vector<vcl_pair<float, vcl_pair<unsigned, unsigned> > > arr;
  for (unsigned ia = 0; ia < na; ia++) {
    for (unsigned ib = 0; ib < nb; ib++) {
      vcl_pair<unsigned, unsigned> ind(ia, ib);
      vcl_pair<float, vcl_pair<unsigned, unsigned> > val(jh.p(ia, ib), ind);
      arr.push_back(val);
    }
  }

  vcl_sort(arr.begin(), arr.end(), first_greater);
  unsigned size = arr.size() < cnt ? arr.size() : cnt;
  for (unsigned i = 0; i < size; i++) {
    //vcl_cout << "sampled from bina: " << arr[i].second.first << " binb: " << arr[i].second.second << "!\n";
    vcl_pair<unsigned, unsigned> out_p(arr[i].second.first, arr[i].second.second);
    out_indices.push_back(out_p);
  }

  return true;
}


#undef BSTA_SAMPLER_INSTANTIATE
#define BSTA_SAMPLER_INSTANTIATE(T) \
template class bsta_sampler<T >

#endif // bsta_sampler_txx_
