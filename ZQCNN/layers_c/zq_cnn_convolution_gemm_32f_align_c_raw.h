/*in_pixStep must be equal to filter_pixStep,
and the aligned channels should be set to zero*/
void zq_cnn_conv_no_padding_gemm_32f_align_same_pixstep(
	const float* in_tensor4D_data,
	int in_N,
	int in_H,
	int in_W,
	int in_C,
	int in_pixelStep,
	int in_widthStep,
	int in_sliceStep,
	const float* filters_data,
	int filter_N,
	int filter_H,
	int filter_W,
	int filter_C, // must be in_C
	int filter_pixelStep,
	int filter_widthStep,
	int filter_sliceStep,
	int stride_H,
	int stride_W,
	float* out_tensor4D_data,
	int out_N,	// must be in_N
	int out_H,	// must be (in_H - filter_H)/stride_H + 1
	int out_W,	// must be (in_W - filter_W)/stride_W + 1
	int out_C,	// must be filter_N
	int out_pixelStep,
	int out_widthStep,
	int out_sliceStep
)
{
	/************** image to col **************/
	int in_widthStep_mul_stride_H = in_widthStep*stride_H;
	int in_pixelStep_mul_stride_W = in_pixelStep*stride_W;
	int filter_pixStep_mul_filter_W = filter_pixelStep*filter_W;
	int matrix_A_cols = filter_sliceStep;
	int matrix_A_rows = out_H*out_W;
	int matrix_B_cols = filter_N;
	int matrix_B_rows = filter_sliceStep;
	float* matrix_A = (float*)_aligned_malloc(matrix_A_rows*matrix_A_cols * sizeof(float), 32);
	const float* in_row_ptr, *in_pix_ptr, *cur_in_row_ptr;
	int out_n, out_h, out_w, kh, pp;
	float* matrix_A_row_ptr, *matrix_A_col_ptr, *cp_dst_ptr;
	const float* cp_src_ptr;
	float* out_slice_ptr, *out_row_ptr, *out_pix_ptr;
	const float* in_slice_ptr;
	const float* matrix_Bt = filters_data;
	float* matrix_C, *matrix_C_row_ptr;
	int out_row_idx;
	int out_HW = out_H*out_W;
	double t1, t2, t3, t4, t5, t6, alloc_time, make_A_time = 0, gemm_time = 0;
	t1 = omp_get_wtime();
	int need_allocate_tmp_out = (out_pixelStep != filter_N) || (out_pixelStep*out_W != out_widthStep) || (out_widthStep*out_H != out_sliceStep);
	if (need_allocate_tmp_out)
	{
		matrix_C = (float*)_aligned_malloc(out_HW*filter_N * sizeof(float), 32);
	}
	else
	{
		matrix_C = out_tensor4D_data;
	}

	t2 = omp_get_wtime();
	alloc_time = t2 - t1;
	for (out_n = 0, in_slice_ptr = in_tensor4D_data, out_slice_ptr = out_tensor4D_data;
		out_n < out_N;
		out_n++, in_slice_ptr += in_sliceStep, out_slice_ptr += out_sliceStep)
	{
		t3 = omp_get_wtime();
		matrix_A_row_ptr = matrix_A;
		for (out_h = 0, in_row_ptr = in_slice_ptr; out_h < out_H; out_h++, in_row_ptr += in_widthStep_mul_stride_H)
		{
			for (out_w = 0, in_pix_ptr = in_row_ptr; out_w < out_W; out_w++, in_pix_ptr += in_pixelStep_mul_stride_W)
			{
				for (kh = 0, cur_in_row_ptr = in_pix_ptr, matrix_A_col_ptr = matrix_A_row_ptr;
					kh < filter_H;
					kh++, cur_in_row_ptr += in_widthStep, matrix_A_col_ptr += filter_pixStep_mul_filter_W)
				{
					//memcpy(matrix_A_col_ptr, cur_in_row_ptr, sizeof(float)*filter_pixStep_mul_filter_W);
					for (pp = 0, cp_src_ptr = cur_in_row_ptr, cp_dst_ptr = matrix_A_col_ptr;
						pp < filter_pixStep_mul_filter_W;
						pp += zq_mm_align_size, cp_src_ptr += zq_mm_align_size, cp_dst_ptr += zq_mm_align_size)
						zq_mm_store_ps(cp_dst_ptr, zq_mm_load_ps(cp_src_ptr));

				}
				matrix_A_row_ptr += matrix_A_cols;
			}
		}
		t4 = omp_get_wtime();
		make_A_time += t4 - t3;
		/*gemm*/
		cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasTrans, matrix_A_rows, matrix_B_cols, matrix_A_cols, 1, matrix_A, matrix_A_cols,
			matrix_Bt, matrix_A_cols, 0.0f, matrix_C, matrix_B_cols);
		t5 = omp_get_wtime();
		gemm_time += t5 - t4;
		if (need_allocate_tmp_out)
		{
			/*   col2im      */
			out_row_idx = 0;
			matrix_C_row_ptr = matrix_C;
			for (out_h = 0, out_row_ptr = out_slice_ptr; out_h < out_H; out_h++, out_row_ptr += out_widthStep)
			{
				for (out_w = 0, out_pix_ptr = out_row_ptr; out_w < out_W; out_w++, out_pix_ptr += out_pixelStep)
				{
					memcpy(out_pix_ptr, matrix_C_row_ptr, sizeof(float)*matrix_B_cols);
					matrix_C_row_ptr += matrix_B_cols;
				}
			}
		}
		else
			matrix_C += out_sliceStep;
		
	}

	if (need_allocate_tmp_out)
		_aligned_free(matrix_C);
	_aligned_free(matrix_A);
	t6 = omp_get_wtime();
	//if (filter_H == 3 && filter_W == 3 && filter_C == 3)
	/*{
		printf("gemm_same_pixstep total: %.3f ms, alloc %.3f ms, makeA: %.3f ms, gemm: %.3f ms\n", 1000 * (t6 - t1), 1000 * alloc_time,
			1000 * make_A_time, 1000 * gemm_time);
	}*/
}

/*in_pixStep must be equal to filter_pixStep,
and the aligned channels should be set to zero*/
void zq_cnn_conv_no_padding_gemm_32f_align_same_pixstep_batch(
	const float* in_tensor4D_data,
	int in_N,
	int in_H,
	int in_W,
	int in_C,
	int in_pixelStep,
	int in_widthStep,
	int in_sliceStep,
	const float* filters_data,
	int filter_N,
	int filter_H,
	int filter_W,
	int filter_C, // must be in_C
	int filter_pixelStep,
	int filter_widthStep,
	int filter_sliceStep,
	int stride_H,
	int stride_W,
	float* out_tensor4D_data,
	int out_N,	// must be in_N
	int out_H,	// must be (in_H - filter_H)/stride_H + 1
	int out_W,	// must be (in_W - filter_W)/stride_W + 1
	int out_C,	// must be filter_N
	int out_pixelStep,
	int out_widthStep,
	int out_sliceStep
)
{
	/************** image to col **************/
	int in_widthStep_mul_stride_H = in_widthStep*stride_H;
	int in_pixelStep_mul_stride_W = in_pixelStep*stride_W;
	int filter_pixStep_mul_filter_W = filter_pixelStep*filter_W;
	int matrix_A_cols = filter_sliceStep;
	int matrix_A_rows = out_N*out_H*out_W;
	int matrix_B_cols = filter_N;
	int matrix_B_rows = filter_sliceStep;
	float* matrix_A = (float*)_aligned_malloc(matrix_A_rows*matrix_A_cols * sizeof(float), 32);
	const float* in_row_ptr, *in_pix_ptr, *cur_in_row_ptr, *cp_src_ptr;
	float *cp_dst_ptr;
	int out_n, out_h, out_w, kh, pp;
	float* matrix_A_row_ptr, *matrix_A_col_ptr;
	float* out_slice_ptr, *out_row_ptr, *out_pix_ptr;
	const float* in_slice_ptr;
	const float* matrix_Bt = filters_data;
	float* matrix_C, *matrix_C_row_ptr;
	int out_row_idx;
	int out_HW = out_H*out_W;
	int out_NHW = out_HW*out_N;
	double t1, t2, t3, t4, t5;
	t1 = omp_get_wtime();
	int need_allocate_tmp_out = (out_pixelStep != filter_N) || (out_pixelStep*out_W != out_widthStep) || (out_widthStep*out_H != out_sliceStep);
	if (need_allocate_tmp_out)
		matrix_C = (float*)_aligned_malloc(out_NHW*filter_N * sizeof(float), 32);
	else
	{
		matrix_C = out_tensor4D_data;
	}

	t2 = omp_get_wtime();
	matrix_A_row_ptr = matrix_A;
	for (out_n = 0, in_slice_ptr = in_tensor4D_data, out_slice_ptr = out_tensor4D_data;
		out_n < out_N;
		out_n++, in_slice_ptr += in_sliceStep, out_slice_ptr += out_sliceStep)
	{
		for (out_h = 0, in_row_ptr = in_slice_ptr; out_h < out_H; out_h++, in_row_ptr += in_widthStep_mul_stride_H)
		{
			for (out_w = 0, in_pix_ptr = in_row_ptr; out_w < out_W; out_w++, in_pix_ptr += in_pixelStep_mul_stride_W)
			{
				for (kh = 0, cur_in_row_ptr = in_pix_ptr, matrix_A_col_ptr = matrix_A_row_ptr;
					kh < filter_H;
					kh++, cur_in_row_ptr += in_widthStep, matrix_A_col_ptr += filter_pixStep_mul_filter_W)
				{
					//memcpy(matrix_A_col_ptr, cur_in_row_ptr, sizeof(float)*filter_pixStep_mul_filter_W);
					for (pp = 0, cp_src_ptr = cur_in_row_ptr, cp_dst_ptr = matrix_A_col_ptr;
						pp < filter_pixStep_mul_filter_W;
						pp += zq_mm_align_size, cp_src_ptr += zq_mm_align_size, cp_dst_ptr += zq_mm_align_size)
						zq_mm_store_ps(cp_dst_ptr, zq_mm_load_ps(cp_src_ptr));

				}
					
				matrix_A_row_ptr += matrix_A_cols;
			}
		}
	}

	t3 = omp_get_wtime();
	/*gemm*/
	cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasTrans, matrix_A_rows, matrix_B_cols, matrix_A_cols, 1, matrix_A, matrix_A_cols,
		matrix_Bt, matrix_A_cols, 0, matrix_C, matrix_B_cols);
	t4 = omp_get_wtime();


	if (need_allocate_tmp_out)
	{
		/*   col2im      */
		out_row_idx = 0;
		matrix_C_row_ptr = matrix_C;
		for (out_n = 0, out_slice_ptr = out_tensor4D_data; out_n < out_N; out_n++, out_slice_ptr += out_sliceStep)
		{
			for (out_h = 0, out_row_ptr = out_slice_ptr; out_h < out_H; out_h++, out_row_ptr += out_widthStep)
			{
				for (out_w = 0, out_pix_ptr = out_row_ptr; out_w < out_W; out_w++, out_pix_ptr += out_pixelStep)
				{
					memcpy(out_pix_ptr, matrix_C_row_ptr, sizeof(float)*matrix_B_cols);
					matrix_C_row_ptr += matrix_B_cols;
				}
			}
		}
	}

	if (need_allocate_tmp_out)
		_aligned_free(matrix_C);
	_aligned_free(matrix_A);
	t5 = omp_get_wtime();
	//if (filter_H == 3 && filter_W == 3 && filter_C == 3)
	/*{
		printf("gemm_same_pixstep_batch total: %.3f ms, alloc %.3f ms, makeA: %.3f ms, gemm: %.3f ms, copy_C: %.3f ms\n", 1000 * (t5 - t1), 1000 * (t2 - t1),
			1000 * (t3 - t2), 1000 * (t4 - t3), 1000 * (t5 - t4));
	}*/
}

/*in_pixStep must be equal to filter_pixStep,
and the aligned channels should be set to zero*/
void zq_cnn_conv_no_padding_gemm_32f_align_same_or_notsame_pixstep(
	const float* in_tensor4D_data,
	int in_N,
	int in_H,
	int in_W,
	int in_C,
	int in_pixelStep,
	int in_widthStep,
	int in_sliceStep,
	const float* filters_data,
	int filter_N,
	int filter_H,
	int filter_W,
	int filter_C, // must be in_C
	int filter_pixelStep,
	int filter_widthStep,
	int filter_sliceStep,
	int stride_H,
	int stride_W,
	float* out_tensor4D_data,
	int out_N,	// must be in_N
	int out_H,	// must be (in_H - filter_H)/stride_H + 1
	int out_W,	// must be (in_W - filter_W)/stride_W + 1
	int out_C,	// must be filter_N
	int out_pixelStep,
	int out_widthStep,
	int out_sliceStep
)
{
	/************** image to col **************/
	int common_align_pixStep = __min(in_pixelStep, filter_pixelStep);
	int in_widthStep_mul_stride_H = in_widthStep*stride_H;
	int in_pixelStep_mul_stride_W = in_pixelStep*stride_W;
	int common_pixStep_mul_filter_W = common_align_pixStep*filter_W;
	int matrix_A_cols = filter_H*filter_W*common_align_pixStep;
	int matrix_A_rows = out_H*out_W;
	int matrix_B_cols = filter_N;
	int matrix_B_rows = filter_H*filter_W*common_align_pixStep;
	float* matrix_A = (float*)_aligned_malloc(matrix_A_rows*matrix_A_cols * sizeof(float), 32);
	float* matrix_Bt;
	const float* in_row_ptr, *in_pix_ptr, *cur_in_row_ptr, *cur_in_pix_ptr, *filter_slice_ptr, *filter_row_ptr, *filter_pix_ptr;
	int out_n, out_h, out_w, kn, kh, kw, pp;
	float* matrix_A_row_ptr, *matrix_A_col_ptr, *cp_dst_ptr;
	const float *cp_src_ptr;
	float* out_slice_ptr, *out_row_ptr, *out_pix_ptr;
	const float* in_slice_ptr;
	
	float* matrix_C, *matrix_C_row_ptr;
	int out_row_idx;
	int out_HW = out_H*out_W;
	double t1, t2, t3, t4, t5, alloc_time, make_A_time =0, gemm_time = 0;
	t1 = omp_get_wtime();
	int need_allocate_tmp_out = (out_pixelStep != filter_N) || (out_pixelStep*out_W != out_widthStep) || (out_widthStep*out_H != out_sliceStep);
	int need_allocate_matrix_Bt = common_align_pixStep != filter_pixelStep;
	if (need_allocate_tmp_out)
	{
		matrix_C = (float*)_aligned_malloc(out_HW*filter_N * sizeof(float), 32);
	}
	else
	{
		matrix_C = out_tensor4D_data;
	}
	if (need_allocate_matrix_Bt)
	{
		matrix_Bt = (float*)_aligned_malloc(matrix_B_rows*matrix_B_cols * sizeof(float), 32);
		cp_dst_ptr = matrix_Bt;
		for (kn = 0, filter_slice_ptr = filters_data; kn < filter_N; kn++, filter_slice_ptr += filter_sliceStep)
		{
			for (kh = 0, filter_row_ptr = filter_slice_ptr; kh < filter_H; kh++, filter_row_ptr += filter_widthStep)
			{
				for (kw = 0, filter_pix_ptr = filter_row_ptr; kw < filter_W; kw++, filter_pix_ptr += filter_pixelStep)
				{
					for (pp = 0, cp_src_ptr = filter_pix_ptr; pp < common_align_pixStep; pp += zq_mm_align_size, cp_src_ptr += zq_mm_align_size)
					{
						zq_mm_store_ps(cp_dst_ptr, zq_mm_load_ps(cp_src_ptr));
						cp_dst_ptr += zq_mm_align_size;
					}
				}
			}
		}
	}
	else
	{
		matrix_Bt = (float*)filters_data;
	}

	t2 = omp_get_wtime();
	alloc_time = t2 - t1;
	for (out_n = 0, in_slice_ptr = in_tensor4D_data, out_slice_ptr = out_tensor4D_data;
		out_n < out_N;
		out_n++, in_slice_ptr += in_sliceStep, out_slice_ptr += out_sliceStep)
	{
		t3 = omp_get_wtime();
		matrix_A_row_ptr = matrix_A;
		for (out_h = 0, in_row_ptr = in_slice_ptr; out_h < out_H; out_h++, in_row_ptr += in_widthStep_mul_stride_H)
		{
			for (out_w = 0, in_pix_ptr = in_row_ptr; out_w < out_W; out_w++, in_pix_ptr += in_pixelStep_mul_stride_W)
			{
				matrix_A_col_ptr = matrix_A_row_ptr;
				for (kh = 0, cur_in_row_ptr = in_pix_ptr;kh < filter_H;	kh++, cur_in_row_ptr += in_widthStep)
				{
					for (kw = 0, cur_in_pix_ptr = cur_in_row_ptr; kw < filter_W; kw++, cur_in_pix_ptr += in_pixelStep)
					{
						for (pp = 0, cp_src_ptr = cur_in_pix_ptr; pp < common_align_pixStep;
							pp += zq_mm_align_size, cp_src_ptr += zq_mm_align_size)
						{
							zq_mm_store_ps(matrix_A_col_ptr, zq_mm_load_ps(cp_src_ptr));
							matrix_A_col_ptr += zq_mm_align_size;
						}
					}
					

				}
				matrix_A_row_ptr += matrix_A_cols;
			}
		}
		t4 = omp_get_wtime();
		make_A_time += t4 - t3;
		/*gemm*/
		cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasTrans, matrix_A_rows, matrix_B_cols, matrix_A_cols, 1, matrix_A, matrix_A_cols,
			matrix_Bt, matrix_A_cols, 0.0f, matrix_C, matrix_B_cols);
		t5 = omp_get_wtime();
		gemm_time += t5 - t4;
		if (need_allocate_tmp_out)
		{
			/*   col2im      */
			out_row_idx = 0;
			matrix_C_row_ptr = matrix_C;
			for (out_h = 0, out_row_ptr = out_slice_ptr; out_h < out_H; out_h++, out_row_ptr += out_widthStep)
			{
				for (out_w = 0, out_pix_ptr = out_row_ptr; out_w < out_W; out_w++, out_pix_ptr += out_pixelStep)
				{
					memcpy(out_pix_ptr, matrix_C_row_ptr, sizeof(float)*matrix_B_cols);
					matrix_C_row_ptr += matrix_B_cols;
				}
			}
		}
		else
			matrix_C += out_sliceStep;
		
	}

	if (need_allocate_tmp_out)
		_aligned_free(matrix_C);
	_aligned_free(matrix_A);
	if (need_allocate_matrix_Bt)
		_aligned_free(matrix_Bt);
	t5 = omp_get_wtime();
	//if (filter_H == 3 && filter_W == 3 && filter_C == 3)
	/*{
		printf("gemm_nosame_pixstep total: %.3f ms, alloc %.3f ms, makeA: %.3f ms, gemm: %.3f ms\n", 1000 * (t5 - t1), 1000 * alloc_time,
			1000 * make_A_time, 1000 * gemm_time);
	}*/
}

/*in_pixStep can be different with filter_pixStep,
and the aligned channels should be set to zero*/
void zq_cnn_conv_no_padding_gemm_32f_align_same_or_notsame_pixstep_C3(
	const float* in_tensor4D_data,
	int in_N,
	int in_H,
	int in_W,
	int in_C,
	int in_pixelStep,
	int in_widthStep,
	int in_sliceStep,
	const float* filters_data,
	int filter_N,
	int filter_H,
	int filter_W,
	int filter_C, // must be in_C
	int filter_pixelStep,
	int filter_widthStep,
	int filter_sliceStep,
	int stride_H,
	int stride_W,
	float* out_tensor4D_data,
	int out_N,	// must be in_N
	int out_H,	// must be (in_H - filter_H)/stride_H + 1
	int out_W,	// must be (in_W - filter_W)/stride_W + 1
	int out_C,	// must be filter_N
	int out_pixelStep,
	int out_widthStep,
	int out_sliceStep
)
{
	/************** image to col **************/
	int common_align_pixStep = __min(in_pixelStep, filter_pixelStep);
	int in_widthStep_mul_stride_H = in_widthStep*stride_H;
	int in_pixelStep_mul_stride_W = in_pixelStep*stride_W;
	int common_pixStep_mul_filter_W = common_align_pixStep*filter_W;
	int matrix_A_cols = filter_H*filter_W*common_align_pixStep;
	int matrix_A_rows = out_H*out_W;
	int matrix_B_cols = filter_N;
	int matrix_B_rows = filter_H*filter_W*common_align_pixStep;
	float* matrix_A = (float*)_aligned_malloc(matrix_A_rows*matrix_A_cols * sizeof(float), 32);
	float* matrix_Bt;
	const float* in_row_ptr, *in_pix_ptr, *cur_in_row_ptr, *cur_in_pix_ptr, *filter_slice_ptr, *filter_row_ptr, *filter_pix_ptr;
	int out_n, out_h, out_w, kn, kh, kw;
	float* matrix_A_row_ptr, *matrix_A_col_ptr, *cp_dst_ptr;
	float* out_slice_ptr, *out_row_ptr, *out_pix_ptr;
	const float* in_slice_ptr;

	float* matrix_C, *matrix_C_row_ptr;
	int out_row_idx;
	int out_HW = out_H*out_W;
	double t1, t2, t3, t4, t5, alloc_time, make_A_time = 0, gemm_time = 0;
	t1 = omp_get_wtime();
	int need_allocate_tmp_out = (out_pixelStep != filter_N) || (out_pixelStep*out_W != out_widthStep) || (out_widthStep*out_H != out_sliceStep);
	int need_allocate_matrix_Bt = common_align_pixStep != filter_pixelStep;
	if (need_allocate_tmp_out)
	{
		matrix_C = (float*)_aligned_malloc(out_HW*filter_N * sizeof(float), 32);
	}
	else
	{
		matrix_C = out_tensor4D_data;
	}
	if (need_allocate_matrix_Bt)
	{
		matrix_Bt = (float*)_aligned_malloc(matrix_B_rows*matrix_B_cols * sizeof(float), 32);
		cp_dst_ptr = matrix_Bt;
		for (kn = 0, filter_slice_ptr = filters_data; kn < filter_N; kn++, filter_slice_ptr += filter_sliceStep)
		{
			for (kh = 0, filter_row_ptr = filter_slice_ptr; kh < filter_H; kh++, filter_row_ptr += filter_widthStep)
			{
				for (kw = 0, filter_pix_ptr = filter_row_ptr; kw < filter_W; kw++, filter_pix_ptr += filter_pixelStep)
				{
					zq_mm_store_ps(cp_dst_ptr, zq_mm_load_ps(filter_pix_ptr));
					cp_dst_ptr += zq_mm_align_size;

				}
			}
		}
	}
	else
	{
		matrix_Bt = (float*)filters_data;
	}

	t2 = omp_get_wtime();
	alloc_time = t2 - t1;
	for (out_n = 0, in_slice_ptr = in_tensor4D_data, out_slice_ptr = out_tensor4D_data;
		out_n < out_N;
		out_n++, in_slice_ptr += in_sliceStep, out_slice_ptr += out_sliceStep)
	{
		t3 = omp_get_wtime();
		matrix_A_row_ptr = matrix_A;
		for (out_h = 0, in_row_ptr = in_slice_ptr; out_h < out_H; out_h++, in_row_ptr += in_widthStep_mul_stride_H)
		{
			for (out_w = 0, in_pix_ptr = in_row_ptr; out_w < out_W; out_w++, in_pix_ptr += in_pixelStep_mul_stride_W)
			{
				matrix_A_col_ptr = matrix_A_row_ptr;
				for (kh = 0, cur_in_row_ptr = in_pix_ptr; kh < filter_H; kh++, cur_in_row_ptr += in_widthStep)
				{
					for (kw = 0, cur_in_pix_ptr = cur_in_row_ptr; kw < filter_W; kw++, cur_in_pix_ptr += in_pixelStep)
					{
						zq_mm_store_ps(matrix_A_col_ptr, zq_mm_load_ps(cur_in_pix_ptr));
						matrix_A_col_ptr += zq_mm_align_size;
					}
				}
				matrix_A_row_ptr += matrix_A_cols;
			}
		}
		t4 = omp_get_wtime();
		make_A_time += t4 - t3;
		/*gemm*/
		cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasTrans, matrix_A_rows, matrix_B_cols, matrix_A_cols, 1, matrix_A, matrix_A_cols,
			matrix_Bt, matrix_A_cols, 0.0f, matrix_C, matrix_B_cols);
		t5 = omp_get_wtime();
		gemm_time += t5 - t4;
		if (need_allocate_tmp_out)
		{
			/*   col2im      */
			out_row_idx = 0;
			matrix_C_row_ptr = matrix_C;
			for (out_h = 0, out_row_ptr = out_slice_ptr; out_h < out_H; out_h++, out_row_ptr += out_widthStep)
			{
				for (out_w = 0, out_pix_ptr = out_row_ptr; out_w < out_W; out_w++, out_pix_ptr += out_pixelStep)
				{
					memcpy(out_pix_ptr, matrix_C_row_ptr, sizeof(float)*matrix_B_cols);
					matrix_C_row_ptr += matrix_B_cols;
				}
			}
		}
		else
			matrix_C += out_sliceStep;

	}

	if (need_allocate_tmp_out)
		_aligned_free(matrix_C);
	_aligned_free(matrix_A);
	if (need_allocate_matrix_Bt)
		_aligned_free(matrix_Bt);
	t5 = omp_get_wtime();
	//if (filter_H == 3 && filter_W == 3 && filter_C == 3)
	/*{
	printf("gemm_nosame_pixstep total: %.3f ms, alloc %.3f ms, makeA: %.3f ms, gemm: %.3f ms\n", 1000 * (t5 - t1), 1000 * alloc_time,
	1000 * make_A_time, 1000 * gemm_time);
	}*/
}


/*in_pixStep can be different with filter_pixStep,
and the aligned channels should be set to zero*/
void zq_cnn_conv_no_padding_gemm_32f_align_same_or_notsame_pixstep_batch(
	const float* in_tensor4D_data,
	int in_N,
	int in_H,
	int in_W,
	int in_C,
	int in_pixelStep,
	int in_widthStep,
	int in_sliceStep,
	const float* filters_data,
	int filter_N,
	int filter_H,
	int filter_W,
	int filter_C, // must be in_C
	int filter_pixelStep,
	int filter_widthStep,
	int filter_sliceStep,
	int stride_H,
	int stride_W,
	float* out_tensor4D_data,
	int out_N,	// must be in_N
	int out_H,	// must be (in_H - filter_H)/stride_H + 1
	int out_W,	// must be (in_W - filter_W)/stride_W + 1
	int out_C,	// must be filter_N
	int out_pixelStep,
	int out_widthStep,
	int out_sliceStep
)
{
	/************** image to col **************/
	int common_align_pixStep = __min(in_pixelStep, filter_pixelStep);
	int in_widthStep_mul_stride_H = in_widthStep*stride_H;
	int in_pixelStep_mul_stride_W = in_pixelStep*stride_W;
	int filter_pixStep_mul_filter_W = filter_pixelStep*filter_W;
	int matrix_A_cols = filter_H*filter_W*common_align_pixStep;
	int matrix_A_rows = out_N*out_H*out_W;
	int matrix_B_cols = filter_N;
	int matrix_B_rows = filter_H*filter_W*common_align_pixStep;
	float* matrix_A = (float*)_aligned_malloc(matrix_A_rows*matrix_A_cols * sizeof(float), 32);
	float* matrix_Bt = 0;
	const float* in_slice_ptr, *in_row_ptr, *in_pix_ptr, *cur_in_row_ptr, *cur_in_pix_ptr, *filter_slice_ptr, *filter_row_ptr, *filter_pix_ptr;
	int out_n, out_h, out_w, kn, kh, kw, pp;
	float* matrix_A_row_ptr, *matrix_A_col_ptr, *cp_dst_ptr;
	const float* cp_src_ptr;
	float* out_slice_ptr, *out_row_ptr, *out_pix_ptr;
	float* matrix_C, *matrix_C_row_ptr;
	int out_row_idx;
	int out_NHW = out_N*out_H*out_W;
	double t1, t2, t3, t4, t5;
	t1 = omp_get_wtime();
	int need_allocate_matrix_Bt = common_align_pixStep != filter_pixelStep;
	int need_allocate_tmp_out = (out_pixelStep != filter_N) || (out_pixelStep*out_W != out_widthStep) || (out_widthStep*out_H != out_sliceStep);
	if (need_allocate_tmp_out)
	{
		matrix_C = (float*)_aligned_malloc(out_NHW*filter_N * sizeof(float), 32);
	}
	else
	{
		matrix_C = out_tensor4D_data;
	}
	if (need_allocate_matrix_Bt)
	{
		matrix_Bt = (float*)_aligned_malloc(matrix_B_rows*matrix_B_cols * sizeof(float), 32);
		cp_dst_ptr = matrix_Bt;
		for (kn = 0, filter_slice_ptr = filters_data; kn < filter_N; kn++, filter_slice_ptr += filter_sliceStep)
		{
			for (kh = 0, filter_row_ptr = filter_slice_ptr; kh < filter_H; kh++, filter_row_ptr += filter_widthStep)
			{
				for (kw = 0, filter_pix_ptr = filter_row_ptr; kw < filter_W; kw++, filter_pix_ptr += filter_pixelStep)
				{
					for (pp = 0, cp_src_ptr = filter_pix_ptr; pp < common_align_pixStep; pp += zq_mm_align_size, cp_src_ptr += zq_mm_align_size)
					{
						zq_mm_store_ps(cp_dst_ptr, zq_mm_load_ps(cp_src_ptr));
						cp_dst_ptr += zq_mm_align_size;
					}
				}
			}
		}
	}
	else
	{
		matrix_Bt = (float*)filters_data;
	}
	t2 = omp_get_wtime();


	matrix_A_row_ptr = matrix_A;
	for (out_n = 0, in_slice_ptr = in_tensor4D_data, out_slice_ptr = out_tensor4D_data;
		out_n < out_N;
		out_n++, in_slice_ptr += in_sliceStep, out_slice_ptr += out_sliceStep)
	{
		for (out_h = 0, in_row_ptr = in_slice_ptr; out_h < out_H; out_h++, in_row_ptr += in_widthStep_mul_stride_H)
		{
			for (out_w = 0, in_pix_ptr = in_row_ptr; out_w < out_W; out_w++, in_pix_ptr += in_pixelStep_mul_stride_W)
			{
				matrix_A_col_ptr = matrix_A_row_ptr;
				for (kh = 0, cur_in_row_ptr = in_pix_ptr;
					kh < filter_H;
					kh++, cur_in_row_ptr += in_widthStep)
				{
					for (kw = 0, cur_in_pix_ptr = cur_in_row_ptr; kw < filter_W; kw++, cur_in_pix_ptr += in_pixelStep)
					{
						for (pp = 0, cp_src_ptr = cur_in_pix_ptr; pp < common_align_pixStep;
							pp += zq_mm_align_size, cp_src_ptr += zq_mm_align_size)
						{
							zq_mm_store_ps(matrix_A_col_ptr, zq_mm_load_ps(cp_src_ptr));
							matrix_A_col_ptr += zq_mm_align_size;
						}
					}
				}
				matrix_A_row_ptr += matrix_A_cols;
			}
		}
	}

	t3 = omp_get_wtime();
	/*gemm*/
	cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasTrans, matrix_A_rows, matrix_B_cols, matrix_A_cols, 1, matrix_A, matrix_A_cols,
		matrix_Bt, matrix_A_cols, 0.0f, matrix_C, matrix_B_cols);
	t4 = omp_get_wtime();
	if (need_allocate_tmp_out)
	{
		/*   col2im      */
		out_row_idx = 0;
		matrix_C_row_ptr = matrix_C;
		for (out_n = 0, out_slice_ptr = out_tensor4D_data; out_n < out_N; out_n++,out_slice_ptr+=out_sliceStep)
		{
			for (out_h = 0, out_row_ptr = out_slice_ptr; out_h < out_H; out_h++, out_row_ptr += out_widthStep)
			{
				for (out_w = 0, out_pix_ptr = out_row_ptr; out_w < out_W; out_w++, out_pix_ptr += out_pixelStep)
				{
					memcpy(out_pix_ptr, matrix_C_row_ptr, sizeof(float)*matrix_B_cols);
					matrix_C_row_ptr += matrix_B_cols;
				}
			}
		}
	}
	else
		matrix_C += out_sliceStep;
	t5 = omp_get_wtime();

	if (need_allocate_tmp_out)
		_aligned_free(matrix_C);
	_aligned_free(matrix_A);
	if (need_allocate_matrix_Bt)
		_aligned_free((float*)matrix_Bt);
	//if (filter_H == 3 && filter_W == 3 && filter_C == 3)
	/*{
		printf("gemm_same_pixstep_batch total: %.3f ms, alloc %.3f ms, makeA: %.3f ms, gemm: %.3f ms, copy_C: %.3f ms\n", 1000 * (t5 - t1), 1000 * (t2 - t1),
			1000 * (t3 - t2), 1000 * (t4 - t3), 1000 * (t5 - t4));
	}*/
}