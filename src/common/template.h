#ifndef TEMPLATE_HELPER_H_
#define TEMPLATE_HELPER_H_
// from https://github.com/glouw/ctl/blob/master/ctl/ctl.h

#define CAT(A, B) A##B

#define PASTE(a, b) CAT(a, b)

#define JOIN(prefix, name) PASTE(prefix, PASTE(_, name))

#define SWAP(TYPE, a, b)                                                       \
  {                                                                            \
    TYPE temp = *(a);                                                          \
    *(a) = *(b);                                                               \
    *(b) = temp;                                                               \
  }

#define foreach(a, b, c)                                                       \
  for (JOIN(a, it) c = JOIN(JOIN(a, it), each)(b); !c.done; c.step(&c))

#endif // TEMPLATE_HELPER_H_
