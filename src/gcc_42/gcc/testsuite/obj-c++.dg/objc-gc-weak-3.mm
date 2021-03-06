/* APPLE LOCAL file radar 4280641 */
/* Check that gcc does not loop when generating 'weak' barriers. */
/* { dg-options "-fobjc-gc" } */
/* { dg-do compile { target *-*-darwin* } } */

template <class FuncImp>
  inline int *getMe()
  {
      return 10;
  }


template <class FuncImp, class ThisImp, class ParentImp>
  inline bool getP()
  {
    getMe<FuncImp>;
    return true;
  }

