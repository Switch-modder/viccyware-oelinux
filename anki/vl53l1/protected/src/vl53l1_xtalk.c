
/*
* This file is part of VL53L1 Protected
*
* Copyright (C) 2016, STMicroelectronics - All Rights Reserved
*
* License terms: STMicroelectronics Proprietary in accordance with licensing
* terms at www.st.com/sla0044
*
* STMicroelectronics confidential
* Reproduction and Communication of this document is strictly prohibited unless
* specifically authorized in writing by STMicroelectronics.
*
*/






































#include "vl53l1_types.h"
#include "vl53l1_platform_log.h"

#include "vl53l1_core_support.h"
#include "vl53l1_error_codes.h"

#include "vl53l1_xtalk.h"
#include "vl53l1_hist_core.h"








#define LOG_FUNCTION_START(fmt, ...) \
	_LOG_FUNCTION_START(VL53L1_TRACE_MODULE_HISTOGRAM, fmt, ##__VA_ARGS__)
#define LOG_FUNCTION_END(status, ...) \
	_LOG_FUNCTION_END(VL53L1_TRACE_MODULE_HISTOGRAM, status, ##__VA_ARGS__)
#define LOG_FUNCTION_END_FMT(status, fmt, ...) \
	_LOG_FUNCTION_END_FMT(VL53L1_TRACE_MODULE_HISTOGRAM, status, fmt, ##__VA_ARGS__)

#define trace_print(level, ...) \
	_LOG_TRACE_PRINT(VL53L1_TRACE_MODULE_HISTOGRAM, \
	level, VL53L1_TRACE_FUNCTION_NONE, ##__VA_ARGS__)


VL53L1_Error VL53L1_xtalk_calibration_process_data(
	VL53L1_xtalk_range_results_t		*pxtalk_results,
	VL53L1_xtalk_histogram_data_t		*pxtalk_shape,
	VL53L1_xtalk_calibration_results_t	*pxtalk_cal)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	VL53L1_xtalk_algo_data_t xtalk_debug;
	VL53L1_xtalk_algo_data_t *pdebug      = &xtalk_debug;
	VL53L1_xtalk_range_data_t *pxtalk_data = NULL;

	VL53L1_histogram_bin_data_t avg_bins;
	VL53L1_histogram_bin_data_t *pavg_bins   = &avg_bins;

	LOG_FUNCTION_START("");






	memcpy(pavg_bins, &(pxtalk_results->central_histogram_avg),
		sizeof(VL53L1_histogram_bin_data_t));




	if (status == VL53L1_ERROR_NONE)

		VL53L1_init_histogram_bin_data_struct(
			0, 0, &(pdebug->VL53L1_PRM_00057));

	if (status == VL53L1_ERROR_NONE)

		VL53L1_init_histogram_bin_data_struct(
			0, 0, &(pdebug->VL53L1_PRM_00058));






	if (status == VL53L1_ERROR_NONE)

		status = VL53L1_FCTN_00047(
			pxtalk_results,
			pdebug,
			&(pxtalk_cal->algo__crosstalk_compensation_x_plane_gradient_kcps),
			&(pxtalk_cal->algo__crosstalk_compensation_y_plane_gradient_kcps));








	if (status == VL53L1_ERROR_NONE) {

		pxtalk_data = &(pxtalk_results->VL53L1_PRM_00005[4]);

		if (pxtalk_data->no_of_samples >  0) {




			if (status == VL53L1_ERROR_NONE) {

				memcpy(&(pdebug->VL53L1_PRM_00057), pavg_bins, sizeof(VL53L1_histogram_bin_data_t));
			}




			status = VL53L1_FCTN_00048(
				 pxtalk_data,
				 pdebug,
				 &(pxtalk_cal->algo__crosstalk_compensation_plane_offset_kcps));




			if (status == VL53L1_ERROR_NONE)
				status = VL53L1_FCTN_00049(
					  pavg_bins,
					  pdebug,
					  pxtalk_data,
					  pxtalk_results->central_histogram__window_start,
					  pxtalk_results->central_histogram__window_end,
					  &(pxtalk_shape->xtalk_shape));

		} else {




			pxtalk_cal->algo__crosstalk_compensation_plane_offset_kcps = 0;




			pdebug->VL53L1_PRM_00059 = 0;



		}
	}




















	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_generate_dual_reflectance_xtalk_samples (
	VL53L1_xtalk_range_results_t	*pxtalk_results,
	uint16_t					expected_target_distance_mm,
	uint8_t					higher_reflectance,
	VL53L1_histogram_bin_data_t	*pxtalk_avg_samples
)
{
















	VL53L1_Error status        = VL53L1_ERROR_NONE;

	VL53L1_histogram_bin_data_t *pzone_avg_1   = &(pxtalk_results->histogram_avg_1[0]);
	VL53L1_histogram_bin_data_t *pzone_avg_2   = &(pxtalk_results->histogram_avg_2[0]);

	VL53L1_histogram_bin_data_t *pxtalk_output = pxtalk_avg_samples;




	int i = 0;




	for (i = 0 ; i < 5 ; i++) {

		if (status == VL53L1_ERROR_NONE)

			VL53L1_init_histogram_bin_data_struct(
				0, 0, pzone_avg_1);

		if (status == VL53L1_ERROR_NONE)

			VL53L1_init_histogram_bin_data_struct(
				0, 0, pzone_avg_2);

		pzone_avg_1++;
		pzone_avg_2++;
	}





	pzone_avg_1 = &(pxtalk_results->histogram_avg_1[0]);
	pzone_avg_2 = &(pxtalk_results->histogram_avg_2[0]);

	for (i = 0 ; i < 5 ; i++) {

		if (status == VL53L1_ERROR_NONE) {

			status = VL53L1_FCTN_00050(
				pzone_avg_1,
				pzone_avg_2,
				expected_target_distance_mm,
				0x01,

				higher_reflectance,
				pxtalk_output
				);




			pzone_avg_1++;
			pzone_avg_2++;
			pxtalk_output++;

		}
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00050(
	VL53L1_histogram_bin_data_t	*pzone_avg_1,
	VL53L1_histogram_bin_data_t 	*pzone_avg_2,
	uint16_t					expected_target_distance,
	uint8_t					subtract_amb,
	uint8_t					higher_reflectance,
	VL53L1_histogram_bin_data_t	*pxtalk_output
)
{























	VL53L1_Error status = VL53L1_ERROR_NONE;

	VL53L1_histogram_bin_data_t  zone_avg_realigned;



	 SUPPRESS_UNUSED_WARNING(pxtalk_output);
	 SUPPRESS_UNUSED_WARNING(expected_target_distance);




	if ((status == VL53L1_ERROR_NONE) && (subtract_amb == 0x01)) {
		VL53L1_FCTN_00037(
				pzone_avg_1,
				pzone_avg_1->VL53L1_PRM_00028);



		pzone_avg_1->VL53L1_PRM_00028 = 0x0;
	}

	if ((status == VL53L1_ERROR_NONE) && (subtract_amb == 0x01)) {
		VL53L1_FCTN_00037(
			pzone_avg_2,
			pzone_avg_2->VL53L1_PRM_00028);



		pzone_avg_2->VL53L1_PRM_00028 = 0x0;
	}







	if (status == VL53L1_ERROR_NONE) {
		if (higher_reflectance == 0x01) {
			VL53L1_FCTN_00004(
				pzone_avg_2,
				pzone_avg_1,
				&zone_avg_realigned);
		} else {




			VL53L1_FCTN_00004(
				pzone_avg_1,
				pzone_avg_2,
				&zone_avg_realigned);





		}
	}













	LOG_FUNCTION_END(status);

	return status;
}

VL53L1_Error VL53L1_FCTN_00049(
		VL53L1_histogram_bin_data_t        *pavg_bins,
		VL53L1_xtalk_algo_data_t           *pdebug,
		VL53L1_xtalk_range_data_t          *pxtalk_data,
		uint8_t                             histogram__window_start,
		uint8_t                             histogram__window_end,
		VL53L1_xtalk_histogram_shape_t     *pxtalk_shape)
{

	VL53L1_Error status        = VL53L1_ERROR_NONE;




	uint32_t ambient_thresh         = 0;




	if (status == VL53L1_ERROR_NONE)

		VL53L1_FCTN_00037(
			pavg_bins,
			pavg_bins->VL53L1_PRM_00028);




	if (status == VL53L1_ERROR_NONE)

		VL53L1_FCTN_00051(
				6,
				pavg_bins->VL53L1_PRM_00028,
				&ambient_thresh);








	if (status == VL53L1_ERROR_NONE)

	   status = VL53L1_FCTN_00052(
			  pavg_bins,
			  ambient_thresh,
			  histogram__window_start,
			  histogram__window_end);




	if (status == VL53L1_ERROR_NONE)
		status =  VL53L1_FCTN_00053(
			 pavg_bins,
			 pxtalk_data,
			 pdebug,
			 pxtalk_shape);


	LOG_FUNCTION_END(status);

	return status;

}


VL53L1_Error VL53L1_FCTN_00047(
	VL53L1_xtalk_range_results_t   *pxtalk_results,
	VL53L1_xtalk_algo_data_t       *pdebug,
	int16_t                        *xgradient,
	int16_t                        *ygradient
	)
{









	VL53L1_Error status        = VL53L1_ERROR_NONE;

	VL53L1_xtalk_range_data_t  *presults_int = NULL;

	int          i                   = 0;

	uint32_t xtalk_per_spad[4];
	int32_t  VL53L1_PRM_00060         = 0;
	int32_t  VL53L1_PRM_00061         = 0;

	uint8_t  result_invalid          = 0;


	LOG_FUNCTION_START("");




	*xgradient = 0;
	*ygradient = 0;





	for (i = 0; i < 4; i++) {
		xtalk_per_spad[i] = 0;
	}




	for (i = 0; i < 4; i++) {

		if (status == VL53L1_ERROR_NONE) {

			presults_int = &(pxtalk_results->VL53L1_PRM_00005[i]);






			if (presults_int->no_of_samples == 0) {



				result_invalid = 1;
				pdebug->VL53L1_PRM_00062[i] = 0;


			} else {

				xtalk_per_spad[i] = presults_int->rate_per_spad_kcps_avg;




				pdebug->VL53L1_PRM_00062[i] = (uint32_t)xtalk_per_spad[i];

			}
		}

	}








	if ((status == VL53L1_ERROR_NONE) && (result_invalid == 0)) {




		if (status == VL53L1_ERROR_NONE) {

			VL53L1_PRM_00060 = ((int32_t)xtalk_per_spad[1]
				- (int32_t)xtalk_per_spad[0]) / (8);
			VL53L1_PRM_00061 = ((int32_t)xtalk_per_spad[3]
				- (int32_t)xtalk_per_spad[2]) / (8);
		}






		if (status == VL53L1_ERROR_NONE) {

			if (VL53L1_PRM_00060 < -32767) {
				VL53L1_PRM_00060 = -32767;
			} else {
				if (VL53L1_PRM_00060 > 32767) {
					VL53L1_PRM_00060 = 32767;
				}
			}

			if (VL53L1_PRM_00061 < -32767) {
				VL53L1_PRM_00061 = -32767;
			} else {
				if (VL53L1_PRM_00061 > 32767) {
					VL53L1_PRM_00061 = 32767;
				}
			}




			pdebug->VL53L1_PRM_00060 = (int16_t)VL53L1_PRM_00060;
			pdebug->VL53L1_PRM_00061 = (int16_t)VL53L1_PRM_00061;
		}

	} else {




		VL53L1_PRM_00060 = 0;
		VL53L1_PRM_00061 = 0;

		pdebug->VL53L1_PRM_00060 = 0;
		pdebug->VL53L1_PRM_00061 = 0;
	}




	if (status == VL53L1_ERROR_NONE) {
		*xgradient = (int16_t)VL53L1_PRM_00060;
		*ygradient = (int16_t)VL53L1_PRM_00061;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00048 (
	VL53L1_xtalk_range_data_t *pxtalk_data,
	VL53L1_xtalk_algo_data_t  *pdebug,
	uint32_t                 *xtalk_mean_offset_kcps
	)
{








	VL53L1_Error status        = VL53L1_ERROR_NONE;

	uint32_t xtalk_per_spad          = 0;
	uint8_t  result_invalid          = 0;

	LOG_FUNCTION_START("");

	*xtalk_mean_offset_kcps          = 0;



	if (pxtalk_data->no_of_samples == 0) {




		result_invalid = 1;




		pdebug->VL53L1_PRM_00059 = 0;

	}





	if ((status == VL53L1_ERROR_NONE) && (result_invalid == 0)) {





		xtalk_per_spad = pxtalk_data->rate_per_spad_kcps_avg >> 2;




		pdebug->VL53L1_PRM_00059 = xtalk_per_spad;



		if (xtalk_per_spad < 0x3FFFF)
			*xtalk_mean_offset_kcps     = (uint32_t)xtalk_per_spad;
		else
			*xtalk_mean_offset_kcps     = 0x3FFFF;

	} else {




		*xtalk_mean_offset_kcps     = 0;
	}

	LOG_FUNCTION_END(status);

	return status;

}




VL53L1_Error VL53L1_FCTN_00053 (
	VL53L1_histogram_bin_data_t	   *phist_data,
	VL53L1_xtalk_range_data_t      *pxtalk_data,
	VL53L1_xtalk_algo_data_t       *pdebug,
	VL53L1_xtalk_histogram_shape_t *pxtalk_histo
	)
{








	VL53L1_Error status            = VL53L1_ERROR_NONE;

	int          i                 = 0;
	uint64_t     bin_data[VL53L1_XTALK_HISTO_BINS];

	LOG_FUNCTION_START("");













	if (status == VL53L1_ERROR_NONE) {

		pxtalk_histo->VL53L1_PRM_00020             = phist_data->VL53L1_PRM_00020;
		pxtalk_histo->cal_config__vcsel_start = phist_data->cal_config__vcsel_start;
		pxtalk_histo->VL53L1_PRM_00022     = phist_data->VL53L1_PRM_00022;
		pxtalk_histo->VL53L1_PRM_00019               = phist_data->VL53L1_PRM_00019;
		pxtalk_histo->time_stamp              = phist_data->time_stamp;
		pxtalk_histo->vcsel_width             = phist_data->vcsel_width;
		pxtalk_histo->zero_distance_phase     = phist_data->zero_distance_phase;
		pxtalk_histo->zone_id                 = phist_data->zone_id;
		pxtalk_histo->VL53L1_PRM_00021          = VL53L1_XTALK_HISTO_BINS;
		pxtalk_histo->phasecal_result__reference_phase = phist_data->phasecal_result__reference_phase;
		pxtalk_histo->phasecal_result__vcsel_start     = phist_data->phasecal_result__vcsel_start;
	}




	if (status == VL53L1_ERROR_NONE) {

		memcpy(&(pdebug->VL53L1_PRM_00058), phist_data, sizeof(VL53L1_histogram_bin_data_t));
	}









	if (status == VL53L1_ERROR_NONE) {

		for (i = 0; i < pxtalk_histo->VL53L1_PRM_00021; i++) {
			if (phist_data->bin_data[i+phist_data->number_of_ambient_bins] > 0) {
				bin_data[i] =
					(((uint64_t)phist_data->bin_data[i+phist_data->number_of_ambient_bins] << 10)
					+ ((uint64_t)pxtalk_data->signal_total_events_avg / 2))
					/ (uint64_t)pxtalk_data->signal_total_events_avg;
			} else {
				bin_data[i] = 0;
			}
		}
	}




	if (status == VL53L1_ERROR_NONE) {

		for (i = 0; i < VL53L1_XTALK_HISTO_BINS; i++) {
			pxtalk_histo->bin_data[i] = (uint32_t)bin_data[i];
		}
	}




	if (status == VL53L1_ERROR_NONE) {

		for (i = 0; i < pxtalk_histo->VL53L1_PRM_00021; i++) {
			pdebug->VL53L1_PRM_00063[i] = pxtalk_histo->bin_data[i];
		}
	}

	LOG_FUNCTION_END(status);

	return status;

}


VL53L1_Error VL53L1_FCTN_00054(
	VL53L1_customer_nvm_managed_t *pcustomer,
	VL53L1_dynamic_config_t       *pdyn_cfg,
	VL53L1_xtalk_histogram_data_t *pxtalk_shape,
	VL53L1_histogram_bin_data_t   *pip_hist_data,
	VL53L1_histogram_bin_data_t   *pop_hist_data,
	VL53L1_histogram_bin_data_t   *pxtalk_count_data)
{







	VL53L1_Error status = VL53L1_ERROR_NONE;




	uint32_t xtalk_rate_kcps = 0;

	LOG_FUNCTION_START("");




	memcpy(pop_hist_data, pip_hist_data, sizeof(VL53L1_histogram_bin_data_t));




	if (status == VL53L1_ERROR_NONE)

		status =
			VL53L1_FCTN_00040(
				pcustomer->algo__crosstalk_compensation_plane_offset_kcps,
				pcustomer->algo__crosstalk_compensation_x_plane_gradient_kcps,
				pcustomer->algo__crosstalk_compensation_y_plane_gradient_kcps,
				0,

				0,

				pip_hist_data->result__dss_actual_effective_spads,


				pdyn_cfg->roi_config__user_roi_centre_spad,
				pdyn_cfg->roi_config__user_roi_requested_global_xy_size,
				&(xtalk_rate_kcps));




	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_FCTN_00041(
			  pip_hist_data,
			  &(pxtalk_shape->xtalk_shape),
			  xtalk_rate_kcps,
			  pxtalk_count_data);




	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_FCTN_00055 (
				pop_hist_data,
				pxtalk_count_data,
				pip_hist_data->number_of_ambient_bins);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00040 (
	uint32_t                       mean_offset,
	int16_t                        xgradient,
	int16_t                        ygradient,
	int8_t                         centre_offset_x,
	int8_t                         centre_offset_y,
	uint16_t                       roi_effective_spads,
	uint8_t                        roi_centre_spad,
	uint8_t                        roi_xy_size,
	uint32_t                      *xtalk_rate_kcps
	)
{







	VL53L1_Error status = VL53L1_ERROR_NONE;

	uint8_t row = 0;
	uint8_t col = 0;




	int16_t  bound_l_x = 0;

	int16_t  bound_r_x = 0;

	int16_t  bound_u_y = 0;

	int16_t  bound_d_y = 0;


	int64_t xtalk_rate_ll = 0;

	int64_t xtalk_rate_ur = 0;


	int64_t xtalk_avg = 0;

	LOG_FUNCTION_START("");

	SUPPRESS_UNUSED_WARNING(roi_effective_spads);








	if (status == VL53L1_ERROR_NONE) {

		VL53L1_decode_row_col(
				roi_centre_spad,
				&row,
				&col);
	}

	trace_print(
			VL53L1_TRACE_LEVEL_DEBUG,
			"    %-48s : %10d\n",
			"Row", row);

	trace_print(
			VL53L1_TRACE_LEVEL_DEBUG,
			"    %-48s : %10d\n",
			"Col", col);




	if (status == VL53L1_ERROR_NONE) {

		if ((((int16_t)roi_xy_size / 16) & 0x01) == 1)
			bound_l_x = (int16_t) col - (((int16_t)roi_xy_size / 32) + 1);
		else
			bound_l_x = (int16_t) col - ((int16_t)roi_xy_size / 32);

		bound_r_x = (int16_t) col + ((int16_t)roi_xy_size / 32);

		if ((((int16_t)roi_xy_size) & 0x01) == 1)
			bound_d_y = (int16_t) row - ((((int16_t)roi_xy_size & 0x0f) / 2) + 1);
		else
			bound_d_y = (int16_t) row - (((int16_t)roi_xy_size & 0x0f) / 2);

		bound_u_y = (int16_t) row + (((int16_t)roi_xy_size & 0xf) / 2);
	}

	trace_print(
			VL53L1_TRACE_LEVEL_DEBUG,
			"    %-48s : %10d\n",
			"Bound_l_x", bound_l_x);
	trace_print(
			VL53L1_TRACE_LEVEL_DEBUG,
			"    %-48s : %10d\n",
			"Bound_r_x", bound_r_x);
	trace_print(
			VL53L1_TRACE_LEVEL_DEBUG,
			"    %-48s : %10d\n",
			"Bound_u_y", bound_u_y);
	trace_print(
			VL53L1_TRACE_LEVEL_DEBUG,
			"    %-48s : %10d\n",
			"Bound_d_y", bound_d_y);









	if (status == VL53L1_ERROR_NONE) {

		bound_l_x =  (2 * bound_l_x) - 15 + (2 * (int16_t)centre_offset_x);
		bound_r_x =  (2 * bound_r_x) - 15 + (2 * (int16_t)centre_offset_x);
		bound_u_y =  (2 * bound_u_y) - 15 + (2 * (int16_t)centre_offset_y);
		bound_d_y =  (2 * bound_d_y) - 15 + (2 * (int16_t)centre_offset_y);
	}

	trace_print(
		VL53L1_TRACE_LEVEL_DEBUG,
		"    %-48s : %10d\n",
		"Bound_l_x", bound_l_x);

	trace_print(
		VL53L1_TRACE_LEVEL_DEBUG,
		"    %-48s : %10d\n",
		"Bound_r_x", bound_r_x);

	trace_print(
		VL53L1_TRACE_LEVEL_DEBUG,
		"    %-48s : %10d\n",
		"Bound_u_y", bound_u_y);

	trace_print(
		VL53L1_TRACE_LEVEL_DEBUG,
		"    %-48s : %10d\n",
		"Bound_d_y", bound_d_y);





	if (status == VL53L1_ERROR_NONE) {

		xtalk_rate_ll  = ((int64_t)bound_l_x * ((int64_t)xgradient)) + ((int64_t)bound_d_y * ((int64_t)ygradient));
		xtalk_rate_ll  = (xtalk_rate_ll + 1) / 2;

		xtalk_rate_ll += ((int64_t)mean_offset * 4);

		xtalk_rate_ur  = ((int64_t)bound_r_x * ((int64_t)xgradient)) + ((int64_t)bound_u_y * ((int64_t)ygradient));
		xtalk_rate_ur  = (xtalk_rate_ur + 1) / 2;

		xtalk_rate_ur += ((int64_t)mean_offset * 4);
	}

	trace_print(
		VL53L1_TRACE_LEVEL_DEBUG,
		"    %-48s : %10d\n",
		"xtalk_rate_ll", xtalk_rate_ll);

	trace_print(
		VL53L1_TRACE_LEVEL_DEBUG,
		"    %-48s : %10d\n",
		"xtalk_rate_ur", xtalk_rate_ur);




	if (status == VL53L1_ERROR_NONE) {


		xtalk_avg = ((xtalk_rate_ll + xtalk_rate_ur) + 1) / 2;
	}

	trace_print(
			VL53L1_TRACE_LEVEL_DEBUG,
			"    %-48s : %10d\n",
			"xtalk_avg", xtalk_avg);




	if (status == VL53L1_ERROR_NONE) {

		if (xtalk_avg < 0)
			xtalk_avg = 0;
	}





	*xtalk_rate_kcps = (uint32_t) xtalk_avg;

	trace_print(
			VL53L1_TRACE_LEVEL_DEBUG,
			"    %-48s : %10d\n",
			"xtalk_rate_kcps", xtalk_avg);

	LOG_FUNCTION_END(status);

	return status;
}



VL53L1_Error VL53L1_FCTN_00041 (
	VL53L1_histogram_bin_data_t    *phist_data,
	VL53L1_xtalk_histogram_shape_t *pxtalk_data,
	uint32_t                        xtalk_rate_kcps,
	VL53L1_histogram_bin_data_t    *pxtalkcount_data
	)
{









	VL53L1_Error status              = VL53L1_ERROR_NONE;

	uint64_t xtalk_events_per_spad = 0;
	uint64_t xtalk_total_events = 0;
	uint64_t xtalk_temp_bin = 0;

	uint8_t  i = 0;

	LOG_FUNCTION_START("");

	trace_print(
			VL53L1_TRACE_LEVEL_DEBUG,
			"    %-48s : %10d\n",
			"pk_duration_internal", phist_data->peak_duration_us);










	if (status == VL53L1_ERROR_NONE) {

		xtalk_events_per_spad = ((((uint64_t)xtalk_rate_kcps * (uint64_t)phist_data->peak_duration_us) + 500) / 1000);
	}

	trace_print(
			VL53L1_TRACE_LEVEL_DEBUG,
			"    %-48s : %10d\n",
			"xtalk_events_per_spad", xtalk_events_per_spad);






	if (status == VL53L1_ERROR_NONE) {

		xtalk_total_events = xtalk_events_per_spad * (uint64_t)phist_data->result__dss_actual_effective_spads;


		xtalk_total_events = (xtalk_total_events)  / 256;


		xtalk_total_events = (xtalk_total_events + 1024) / 2048;


		if (xtalk_total_events > 0xFFFFFFFF) {
			xtalk_total_events = 0xFFFFFFFF;
		}
	}

	trace_print(
			VL53L1_TRACE_LEVEL_DEBUG,
			"    %-48s : %10d\n",
			"xtalk_total_events", xtalk_total_events);









	if (status == VL53L1_ERROR_NONE) {

		for (i = 0; i < pxtalk_data->VL53L1_PRM_00021; i++) {
			xtalk_temp_bin = (uint64_t)pxtalk_data->bin_data[i] * (uint64_t)xtalk_total_events;
			xtalk_temp_bin = (xtalk_temp_bin + 512) / 1024;

			pxtalkcount_data->bin_data[i] = (uint32_t)xtalk_temp_bin;

			trace_print(
				VL53L1_TRACE_LEVEL_DEBUG,
				"    %-48s : %10d\n",
				"bin_data", pxtalkcount_data->bin_data[i]);
		}
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00055 (
	VL53L1_histogram_bin_data_t	*phist_data,
	VL53L1_histogram_bin_data_t	*pxtalk_data,
	uint8_t					xtalk_bin_offset)
{







	VL53L1_Error status = VL53L1_ERROR_NONE;

	uint8_t  i = 0;

	int32_t  temp_bin;

	LOG_FUNCTION_START("");






	if (status == VL53L1_ERROR_NONE) {

		for (i = xtalk_bin_offset; i < pxtalk_data->VL53L1_PRM_00021; i++) {


			temp_bin = (int32_t)phist_data->bin_data[i] - (int32_t)pxtalk_data->bin_data[i - xtalk_bin_offset];

			if (temp_bin < 0)
				temp_bin = 0;

			phist_data->bin_data[i] = (uint32_t)temp_bin;
		}
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00052(
	VL53L1_histogram_bin_data_t		*pxtalk_data,
	uint32_t						amb_threshold,
	uint8_t						VL53L1_PRM_00019,
	uint8_t						VL53L1_PRM_00024)
{








	VL53L1_Error status = VL53L1_ERROR_NONE;

	uint8_t i = 0;
	uint8_t first_bin_int = 0;
	uint8_t first_bin_inc = 0;
	uint8_t last_bin_int  = 0;
	uint8_t realign_bin   = 0;
	uint8_t realign_index = 0;
	int32_t realign_bin_data[VL53L1_HISTOGRAM_BUFFER_SIZE];

	LOG_FUNCTION_START("");




	for (i = 0 ; i < VL53L1_HISTOGRAM_BUFFER_SIZE ; i++) {
		realign_bin_data[i] = 0;
	}

	first_bin_int = VL53L1_PRM_00019;
	last_bin_int  = VL53L1_PRM_00024;












	VL53L1_hist_remove_ambient_bins(pxtalk_data);




	first_bin_int = (first_bin_int) %
				pxtalk_data->VL53L1_PRM_00021;

	last_bin_int = (last_bin_int) %
				pxtalk_data->VL53L1_PRM_00021;

	first_bin_inc = (first_bin_int + 1) % pxtalk_data->VL53L1_PRM_00021;








	if (first_bin_inc > last_bin_int) {



		realign_bin = pxtalk_data->VL53L1_PRM_00021 - first_bin_inc;



		first_bin_int = (first_bin_int + realign_bin) %
				pxtalk_data->VL53L1_PRM_00021;
		last_bin_int = (last_bin_int + realign_bin) %
				pxtalk_data->VL53L1_PRM_00021;



		pxtalk_data->zero_distance_phase = pxtalk_data->zero_distance_phase +
			((uint16_t)realign_bin * 2048);
	}

	if (realign_bin > 0) {


		for (i = 0; i < pxtalk_data->VL53L1_PRM_00021; i++) {
			realign_bin_data[i] = pxtalk_data->bin_data[i];
		}



		for (i = 0; i < pxtalk_data->VL53L1_PRM_00021; i++) {
			realign_index = (pxtalk_data->VL53L1_PRM_00021 - realign_bin + i)
				% pxtalk_data->VL53L1_PRM_00021;

			pxtalk_data->bin_data[i] = realign_bin_data[realign_index];
		}
	}










	trace_print(
		VL53L1_TRACE_LEVEL_DEBUG,
		"    %-48s : %10d\n",
		"first bin int", first_bin_int);

	trace_print(
		VL53L1_TRACE_LEVEL_DEBUG,
		"    %-48s : %10d\n",
		"last bin int", last_bin_int);

	trace_print(
		VL53L1_TRACE_LEVEL_DEBUG,
		"    %-48s : %10d\n",
		"amb thresh", amb_threshold);







	if (status == VL53L1_ERROR_NONE) {

		for (i = 0; i < pxtalk_data->VL53L1_PRM_00021; i++) {


			if (first_bin_int <= last_bin_int) {
				if ((i >= first_bin_int) && (i <= last_bin_int)) {
					if (pxtalk_data->bin_data[i] < (int32_t)amb_threshold)
						pxtalk_data->bin_data[i] = 0;
				} else {
					pxtalk_data->bin_data[i] = 0;
				}
			} else {
				if ((i >= first_bin_int) || (i <= last_bin_int)) {
					if (pxtalk_data->bin_data[i] < (int32_t)amb_threshold) {
						pxtalk_data->bin_data[i] = 0;
					}
				} else {
					pxtalk_data->bin_data[i] = 0;
				}
			}
		}
	}










	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00051(
		uint8_t                      sigma_mult,
		int32_t                      VL53L1_PRM_00028,
		uint32_t                    *ambient_noise)
{






	VL53L1_Error status              = VL53L1_ERROR_NONE;

	uint32_t ambient_events_per_bin_int = 0;

	LOG_FUNCTION_START("");

	if (VL53L1_PRM_00028 <= 0) {
		ambient_events_per_bin_int = 1;
	} else  {
		ambient_events_per_bin_int = (uint32_t)VL53L1_PRM_00028;
	}

	*ambient_noise =  VL53L1_isqrt(ambient_events_per_bin_int);

	*ambient_noise = *ambient_noise * (uint32_t)sigma_mult;

	LOG_FUNCTION_END(status);

	return status;
}
































