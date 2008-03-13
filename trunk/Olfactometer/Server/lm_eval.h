#ifndef LM_EVAL_H
#define LM_EVAL_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
        double* user_t;
        double* user_y;
        void  * user_arg;
        double (*user_func)( void *arg, double user_t_point, double* par );
} lm_data_type;

void lm_evaluate_default( double* par, int m_dat, double* fvec, 
                          void *data, int *info );

void lm_print_default( int n_par, double* par, int m_dat, double* fvec,
                       void *data, int iflag, int iter, int nfev );

#ifdef __cplusplus
}
#endif

#endif
