#include "rasterizer.hpp"
#include <iostream>
#include <math.h>
#include <stdexcept>

namespace rst {
	rasterizer::rasterizer(int w, int h) : width(w), height(h) {
		frame_buf.resize(w * h);
		depth_buf.resize(w * h);
		sample_frame_buf.resize(w * h * totalSamplePoint);
		sample_depth_buf.resize(w * h * totalSamplePoint);
	}

	pos_buf_id rasterizer::load_positions(const std::vector<Vector3f>& positions) {
		auto id = get_next_id();
		pos_buf.emplace(id, positions);
		return { id };
	}

	ind_buf_id rasterizer::load_indices(const std::vector<Vector3i>& indices) {
		auto id = get_next_id();
		ind_buf.emplace(id, indices);
		return { id };
	}

	col_buf_id rasterizer::load_colors(const std::vector<Vector3f>& color) {
		auto id = get_next_id();
		col_buf.emplace(id, color);
		return { id };
	}

	void rasterizer::set_pixel(const Vector3f& point, const Vector3f& color) {
		if (point.x() < 0 || point.x() >= width ||
			point.y() < 0 || point.y() >= height) return;
		
		auto ind = get_index(point.x(), point.y());

		Vector3f final;
		float tmp = std::max(0.0f, std::min(color[0], 1.0f));
		final[0] = static_cast<int>(std::round(tmp * 255.0f));
		tmp = std::max(0.0f, std::min(color[1], 1.0f));
		final[1] = static_cast<int>(std::round(tmp * 255.0f));
		tmp = std::max(0.0f, std::min(color[2], 1.0f));
		final[2] = static_cast<int>(std::round(tmp * 255.0f));
		frame_buf[ind] = final;
	}

	void rasterizer::set_pixel(int index, const Vector3f& color) {
		if (index < 0 || index >= frame_buf.size()) {
			std::cout << "set_pixel index is wrong!" << std::endl;
			return;
		}

		Vector3f final;
		float tmp = std::max(0.0f, std::min(color[0], 1.0f));
		final[0] = static_cast<int>(std::round(tmp * 255.0f));
		tmp = std::max(0.0f, std::min(color[1], 1.0f));
		final[1] = static_cast<int>(std::round(tmp * 255.0f));
		tmp = std::max(0.0f, std::min(color[2], 1.0f));
		final[2] = static_cast<int>(std::round(tmp * 255.0f));
		frame_buf[index] = final;
	}

	// 灞忓箷绌洪棿鐨勫師鐐规槸宸︿笅瑙?
	// 浣嗘槸瀛樺偍鐨刦ramebuffer 鏄粠宸﹀埌鍙筹紝浠庝笂鍒颁笅
	int rasterizer::get_index(int x, int y) {
		return (height - y - 1) * width + x;
	}

	int rasterizer::get_sample_index(int x, int y, int sx, int sy) {
		int samples_per_pixel = sampleTimes * sampleTimes;
		int pixel_index = (height - y - 1) * width + x;
		int sample_offset = (sy * sampleTimes + sx);  // 鍍忕礌鍐呯殑閲囨牱鐐圭储寮?
		return pixel_index * samples_per_pixel + sample_offset;
	}

	void rasterizer::clear(Buffers buff) {
		if ((buff & Buffers::Color) == Buffers::Color) {
			std::fill(frame_buf.begin(), frame_buf.end(), Eigen::Vector3f{ 0,0,0 });
		}
		if ((buff & Buffers::Depth) == Buffers::Depth) {
			std::fill(depth_buf.begin(), depth_buf.end(), std::numeric_limits<float>::max());
		}
		if ((buff & rst::Buffers::SampleColor) == rst::Buffers::SampleColor)
		{
			std::fill(sample_frame_buf.begin(), sample_frame_buf.end(), Eigen::Vector3f{ 0,0,0 });
		}
		if ((buff & rst::Buffers::SampleDepth) == rst::Buffers::SampleDepth)
		{
			std::fill(sample_depth_buf.begin(), sample_depth_buf.end(), std::numeric_limits<float>::max());
		}
	}

	void rasterizer::set_model(const Matrix4f& m) {
		model = m;
	}

	void rasterizer::set_view(const Matrix4f& v) {
		view = v;
	}

	void rasterizer::set_projection(const Matrix4f& p) {
		projection = p;
	}

	void rst::rasterizer::rasterize_wireframe(const Triangle& t)
	{
		draw_line(t.c(), t.a());
		draw_line(t.c(), t.b());
		draw_line(t.b(), t.a());
	}

	auto to_vec4(const Eigen::Vector3f& v3, float w = 1.0f)
	{
		return Vector4f(v3.x(), v3.y(), v3.z(), w);
	}

	// Bresenham 绾挎缁樺埗绠楁硶锛屽彧鐢ㄦ暣鏁拌繍绠楋紝鏉ュ喅瀹氫笅涓€涓儚绱犳槸鍚戜笅杩樻槸鍚戝彸/宸︼紝閬垮厤鏈寸礌鏂规硶锛坹=mx+b锛変腑鐨勬诞鐐硅繍绠?
	// 鏁存暟鍔犲噺銆佷綅杩愮畻锛?锛? 鏁存暟涔樻硶锛?-4锛? 绫诲瀷杞崲锛?-5锛? 娴偣鍔犮€佸噺銆佷箻銆佸彇鏁达紙4-5锛? 娴偣闄ゆ硶锛?0-25锛? 鏁存暟闄ゆ硶锛?0-80锛?
	// 涓轰粈涔堟诞鐐归櫎娉曟瘮鏁存暟闄ゆ硶杩樿蹇竴鐐癸紵鍥犱负娴偣闄ゆ硶鏈変笓闂ㄤ紭鍖栫殑纭欢锛屼笖娴偣闄ゆ硶鍒拌揪鐩爣绮惧害灏卞緱鍋滄
	void rasterizer::draw_line(Vector3f begin, Vector3f end) {
		//TODO
		auto x1 = begin.x();
		auto y1 = begin.y();
		auto x2 = end.x();
		auto y2 = end.y();

		Vector3f line_color = { 1.0, 1.0, 1.0 };
		int x, y, dx, dy, dx1, dy1, px, py, xe, ye, i;
		dx = x2 - x1;
		dy = y2 - y1;
		dx1 = fabs(dx);
		dy1 = fabs(dy);
		px = 2 * dy1 - dx1;
		py = 2 * dx1 - dy1;

		if (dy1 <= dx1)
		{
			if (dx >= 0)
			{
				x = x1;
				y = y1;
				xe = x2;
			}
			else
			{
				x = x2;
				y = y2;
				xe = x1;
			}
			Eigen::Vector3f point = Eigen::Vector3f(x, y, 1.0f);
			set_pixel(point, line_color);
			for (i = 0; x < xe; i++)
			{
				x = x + 1;
				if (px < 0)
				{
					px = px + 2 * dy1;
				}
				else
				{
					if ((dx < 0 && dy < 0) || (dx > 0 && dy > 0))
					{
						y = y + 1;
					}
					else
					{
						y = y - 1;
					}
					px = px + 2 * (dy1 - dx1);
				}
				//            delay(0);
				Eigen::Vector3f point = Eigen::Vector3f(x, y, 1.0f);
				set_pixel(point, line_color);
			}
		}
		else
		{
			if (dy >= 0)
			{
				x = x1;
				y = y1;
				ye = y2;
			}
			else
			{
				x = x2;
				y = y2;
				ye = y1;
			}
			Eigen::Vector3f point = Eigen::Vector3f(x, y, 1.0f);
			set_pixel(point, line_color);
			for (i = 0; y < ye; i++)
			{
				y = y + 1;
				if (py <= 0)
				{
					py = py + 2 * dx1;
				}
				else
				{
					if ((dx < 0 && dy < 0) || (dx > 0 && dy > 0))
					{
						x = x + 1;
					}
					else
					{
						x = x - 1;
					}
					py = py + 2 * (dx1 - dy1);
				}
				//            delay(0);
				Eigen::Vector3f point = Eigen::Vector3f(x, y, 1.0f);
				set_pixel(point, line_color);
			}
		}
	}

	void rasterizer::draw(pos_buf_id pos_buffer, ind_buf_id ind_buffer,
		col_buf_id col_buffer, Primitive type) {
		//TODO
		auto& buf = pos_buf[pos_buffer.pos_id];
		auto& ind = ind_buf[ind_buffer.ind_id];
		auto& col = col_buf[col_buffer.col_id];

		float f1 = (100 - 0.1) / 2.0;
		float f2 = (100 + 0.1) / 2.0;

		Matrix4f mvp = projection * view * model;
		for (auto& i : ind) {
			Triangle t;
			Vector4f v[] = {
				mvp * to_vec4(buf[i[0]], 1.0f),
				mvp * to_vec4(buf[i[1]], 1.0f),
				mvp * to_vec4(buf[i[2]], 1.0f)
			};
			//椤剁偣鐫€鑹?end

			//姣忎釜鍥惧厓涓€娆?鍥惧厓瑁呴厤鍜屽厜鏍呭寲 start
			//閫忚闄ゆ硶
			for (int i = 0; i < 3; ++i) {
				auto& vec = v[i];
				t.setClipW(i, vec.w());
				vec /= vec.w();
			}

			//灏?x銆亂浠庡師鏈殑 [-1,1] -> [0, width]
			//z浠庡師鏈殑 [-1锛?] -> [0, 100]
			//浠嶯DC鍒板睆骞曠┖闂?
			for (auto& vert : v) {
				vert.x() = 0.5 * width * (vert.x() + 1.0);
				vert.y() = 0.5 * height * (vert.y() + 1.0);
				vert.z() = vert.z() * f1 + f2;
			}
			for (int i = 0; i < 3; ++i)
			{
				t.setVertex(i, v[i].head<3>());
				t.setVertex(i, v[i].head<3>());
				t.setVertex(i, v[i].head<3>());
			}
			//灏唒os_buffer涓殑妯″瀷鍧愭爣杞崲涓轰簡灞忓箷鍧愭爣锛屽苟淇濆瓨鍒?triangle 鍙橀噺涓?

			auto col_x = col[i[0]];
			auto col_y = col[i[1]];
			auto col_z = col[i[2]];

			t.setColor(0, col_x[0], col_x[1], col_x[2]);
			t.setColor(1, col_y[0], col_y[1], col_y[2]);
			t.setColor(2, col_z[0], col_z[1], col_z[2]);

			if (type == Primitive::Triangle) {
#ifdef MSAA_OR_SSAA
				if (MSAA_OR_SSAA == 0) {
					rasterize_triangle_MSAA(t);
				}
				else {
					rasterize_triangle_SSAA(t);
				}
#else
				rasterize_triangle(t);
#endif
			}
			else {
				rasterize_wireframe(t);
			}
		}

		// 鍏跺疄搴旇鏄瘡涓€甯х殑鏈€鍚?resolve
		if (type == Primitive::Triangle) {
#ifdef MSAA_OR_SSAA
			resolve_frame();
#endif
		}
	}

	void rasterizer::draw(std::vector<Triangle*>& TriangleList) {
		float f1 = (50 - 0.1) / 2.0;
		float f2 = (50 + 0.1) / 2.0;

		Matrix4f mv = view * model;
		Matrix4f mvp = projection * mv;
		for (const auto& t : TriangleList) {
			Triangle newtri = *t;

			std::array<Vector4f, 3> mm{
				(mv * to_vec4(t->v[0], 1.0f)),
				(mv * to_vec4(t->v[1], 1.0f)),
				(mv * to_vec4(t->v[2], 1.0f))
			};

			std::array<Vector3f, 3> viewspace_pos;
			std::transform(mm.begin(), mm.end(), viewspace_pos.begin(), [](auto& v) {
				return v.template head<3>();
			});

			Vector4f v[] = {
				mvp * to_vec4(t->v[0], 1.0f),
				mvp * to_vec4(t->v[1], 1.0f),
				mvp * to_vec4(t->v[2], 1.0f)
			};

			Eigen::Matrix4f inv_trans = mv.inverse().transpose();
			//inverse 鏄€嗙煩闃点€乼ranspose 鏄浆缃搷浣?
			Eigen::Vector4f n[] = {
					inv_trans * to_vec4(t->normal[0], 0.0f),
					inv_trans * to_vec4(t->normal[1], 0.0f),
					inv_trans * to_vec4(t->normal[2], 0.0f)
			};
			// 灏嗘ā鍨嬬┖闂翠腑鐨刵ormal杞崲鍒拌瀵熺┖闂?

			for (int i = 0; i < 3; ++i) {
				auto& vec = v[i];
				newtri.setClipW(i, vec.w());
				vec /= vec.w();
			}

			//Viewport transformation
			for (auto& vert : v) {
				vert.x() = 0.5 * width * (vert.x() + 1.0);
				vert.y() = 0.5 * height * (vert.y() + 1.0);
				vert.z() = vert.z() * f1 + f2;
			}
			for (int i = 0; i < 3; ++i)
			{
				newtri.setVertex(i, v[i].head<3>());
			}

			for (int i = 0; i < 3; ++i)
			{
				//view space normal
				newtri.setNormal(i, n[i].head<3>());
				newtri.setColor(i, {0.5,0.5,0.5}); //榛樿鐏拌壊
			}

#ifdef MSAA_OR_SSAA
			rasterize_triangle_MSAA(newtri, viewspace_pos);
#else
			rasterize_triangle(newtri, viewspace_pos);
#endif
		}

		
#ifdef MSAA_OR_SSAA
		// 鍏跺疄搴旇鏄瘡涓€甯х殑鏈€鍚?resolve
		resolve_frame();
#endif
	}

	void rst::rasterizer::rasterize_triangle(const Triangle& t, const std::array<Vector3f, 3>& view_pos) {
		auto v = t.toVector4();

		float minx = std::min({ v[0].x(), v[1].x(), v[2].x() });
		float maxx = std::max({ v[0].x(), v[1].x(), v[2].x() });
		float miny = std::min({ v[0].y(), v[1].y(), v[2].y() });
		float maxy = std::max({ v[0].y(), v[1].y(), v[2].y() });
		int screen_min_x = static_cast<int>(std::floor(minx));
		int screen_max_x = static_cast<int>(std::ceil(maxx));
		int screen_min_y = static_cast<int>(std::floor(miny));
		int screen_max_y = static_cast<int>(std::ceil(maxy));
		screen_min_x = std::max(0, screen_min_x);
		screen_max_x = std::min(width - 1, screen_max_x);
		screen_min_y = std::max(0, screen_min_y);
		screen_max_y = std::min(height - 1, screen_max_y);
		if (screen_min_x > screen_max_x || screen_min_y > screen_max_y) {
			return;
		}

		std::vector<Texture*> texture_ptrs;
		texture_ptrs.reserve(textures.size());
		for (auto& tex : textures)
			texture_ptrs.push_back(tex ? &(*tex) : nullptr);

		#pragma omp parallel for collapse(2) schedule(dynamic)
		for (int x = screen_min_x; x <= screen_max_x; x++) {
			for (int y = screen_min_y; y <= screen_max_y; y++) {
				float pixel_x = x + 0.5f;
				float pixel_y = y + 0.5f;

				if (insideTriangle(pixel_x, pixel_y, t.v)) {
					auto [alpha, beta, gamma] = computeBarycentric2D(pixel_x, pixel_y, t.v);
					float w_reciprocal = 1.0 / (alpha / v[0].w() + beta / v[1].w() + gamma / v[2].w());
					float z_interpolated = alpha * v[0].z() / v[0].w() + beta * v[1].z() / v[0].w() + gamma * v[2].z() / v[2].w();
					z_interpolated *= w_reciprocal;

					int index = get_index(x, y);
					if (z_interpolated < depth_buf[index]) {
						depth_buf[index] = z_interpolated;

						auto interpolated_color = interpolate(alpha, beta, gamma, t.color, t.getClipW(), w_reciprocal);
						//鐩存帴浠庨《鐐瑰睘鎬х殑棰滆壊灞炴€т腑鎻掑€?
						auto interpolated_normal = interpolate(alpha, beta, gamma, t.normal, t.getClipW(), w_reciprocal);
						auto interpolated_texcoords = interpolate(alpha, beta, gamma, t.tex_coords, t.getClipW(), w_reciprocal);
						auto interpolated_shadingcoords = interpolate(alpha, beta, gamma, view_pos, t.getClipW(), w_reciprocal);

						fragment_shader_payload payload(interpolated_color, interpolated_normal.normalized(),
							interpolated_texcoords, texture_ptrs);

						payload.view_pos = interpolated_shadingcoords;
						auto pixel_color = fragment_shader(payload);

						set_pixel(index, pixel_color);
					}
				}
			}
		}
	}

	void rst::rasterizer::rasterize_triangle(const Triangle& t) {
		//TODO 濉厖涓夎褰?
		//涓嶄細鏄厛姹傚緱涓夎褰㈢殑鍖呭洿鐩?鐒跺悗閬嶅巻鍖呭洿鐩掍腑鐨勬瘡涓偣锛岃绠楁槸鍚﹀湪涓夎褰㈠唴閮?
		//鍦ㄥ唴閮ㄥ氨涓婅壊

		//涓婅壊鐨勬椂鍊欓渶瑕佸姣?z-buffer 涓殑娣卞害鍊?

		//鍚屾椂閽堝鍦ㄤ笁瑙掑舰杈圭紭鐨勫儚绱犻噰鐢?MSAA 鐨勭畻娉曪紝妯＄硦涓€涓嬶紒

		auto v = t.toVector4();

		//1.鍒涘缓灞忓箷鍧愭爣鐨勪簩缁村寘鍥寸洅
		float minx = std::min({ v[0].x(), v[1].x(), v[2].x() });
		float maxx = std::max({ v[0].x(), v[1].x(), v[2].x() });
		float miny = std::min({ v[0].y(), v[1].y(), v[2].y() });
		float maxy = std::max({ v[0].y(), v[1].y(), v[2].y() });
		int screen_min_x = static_cast<int>(std::floor(minx));
		int screen_max_x = static_cast<int>(std::ceil(maxx));
		int screen_min_y = static_cast<int>(std::floor(miny));
		int screen_max_y = static_cast<int>(std::ceil(maxy));
		screen_min_x = std::max(0, screen_min_x);
		screen_max_x = std::min(width - 1, screen_max_x);
		screen_min_y = std::max(0, screen_min_y);
		screen_max_y = std::min(height - 1, screen_max_y);
		if (screen_min_x > screen_max_x || screen_min_y > screen_max_y) {
			return;
		}

		//2.閬嶅巻鍖呭洿鐩掍腑鐨勬瘡涓€涓偣锛屽垽鏂槸鍚﹀湪涓夎褰㈠唴
		for (int x = screen_min_x; x <= screen_max_x; x++) {
			for (int y = screen_min_y; y <= screen_max_y; y++) {
				if (insideTriangle(x, y, t.v)) {
					auto [alpha, beta, gamma] = computeBarycentric2D(x, y, t.v);
					float w_reciprocal = 1.0f / (alpha / v[0].w() + beta / v[1].w() + gamma / v[2].w());
					float z_interpolated = (alpha * v[0].z() / v[0].w() +
						beta * v[1].z() / v[1].w() +
						gamma * v[2].z() / v[2].w()) * w_reciprocal;

					auto index = get_index(x, y);
					if (z_interpolated < depth_buf[index]) {
						depth_buf[index] = z_interpolated;
						Vector3f interpolated_color = (
							alpha * t.color[0] / v[0].w() +
							beta * t.color[1] / v[1].w() +
							gamma * t.color[2] / v[2].w()
							) * w_reciprocal;
						set_pixel(index, interpolated_color);
					}
				}
			}
		}
	}

	// 涓€涓猟rawcall鍙兘鍖呭惈澶氫釜涓夎褰?

	// 鍐欎簡 MSAA 鍜?SSAA 涓ょ鏂规硶
	// 缃戜笂璇?MSAA 姣忎釜鍍忕礌鍙墽琛屼竴娆＄潃鑹插櫒锛岃€?SSAA 姣忎釜閲囨牱鐐归兘鎵ц浜嗕竴娆＄潃鑹?
	// 鍦ㄥ疄鐜扮殑鏃跺€欙紝姣忎釜閲囨牱鐐归兘淇濆瓨浜嗛鑹茬紦鍐诧紙杩欐槸涓轰簡瑙ｅ喅涓嶅悓鐨勪笁瑙掑舰瑕嗙洊鐨勯棶棰橈級
	// 浣嗘槸 MSAA 涓€涓儚绱犲唴鐨勯噰鏍风偣鍙绠椾簡涓€娆￠鑹诧紝
	// 涔熷氨鏄竴涓儚绱犲唴MSAA瑕佷箞涓嶅啓锛岃鍐欑殑璇濋兘鏄竴涓鑹插€硷紱鑰孲SAA姣忎釜鐫€鑹茬偣閮借绠椾簡涓€娆￠鑹?

	//MSAA
	void rst::rasterizer::rasterize_triangle_MSAA(const Triangle& t) {
		auto v = t.toVector4();

		//鏍规嵁灞忓箷绌洪棿鍧愭爣璁＄畻浜岀淮鍖呭洿鐩?
		float minx = std::min({ v[0].x(), v[1].x(), v[2].x() });
		float maxx = std::max({ v[0].x(), v[1].x(), v[2].x() });
		float miny = std::min({ v[0].y(), v[1].y(), v[2].y() });
		float maxy = std::max({ v[0].y(), v[1].y(), v[2].y() });
		int screen_min_x = static_cast<int>(std::floor(minx));
		int screen_max_x = static_cast<int>(std::ceil(maxx));
		int screen_min_y = static_cast<int>(std::floor(miny));
		int screen_max_y = static_cast<int>(std::ceil(maxy));
		screen_min_x = std::max(0, screen_min_x);
		screen_max_x = std::min(width - 1, screen_max_x);
		screen_min_y = std::max(0, screen_min_y);
		screen_max_y = std::min(height - 1, screen_max_y);
		if (screen_min_x > screen_max_x || screen_min_y > screen_max_y) {
			return;
		}
		//濡傛灉杩欎釜涓夎褰㈡湁閮ㄥ垎鍦ㄥ睆骞曠┖闂翠互澶栵紝鍙互瑁佸壀杩欎釜涓夎褰?

		for (int x = screen_min_x; x <= screen_max_x; x++) {
			for (int y = screen_min_y; y <= screen_max_y; y++) {
				Vector3f interpolated_color;
				auto pixelCoverage = checkPixelCoverage(x, y, t.v);
				if (pixelCoverage == PixelCoverage::FULLY_OUTSIDE) {
					continue;
				}
				if (pixelCoverage == PixelCoverage::FULLY_INSIDE) {
					int sample_index = get_sample_index(x, y, 0, 0);
					auto barycentric = computeBarycentric2D(x, y, t.v);
					float alpha = std::get<0>(barycentric);
					float beta = std::get<1>(barycentric);
					float gamma = std::get<2>(barycentric);
					float w_reciprocal = 1.0f / (alpha / v[0].w() + beta / v[1].w() + gamma / v[2].w());
					float z_interpolated = (alpha * v[0].z() / v[0].w() +
						beta * v[1].z() / v[1].w() +
						gamma * v[2].z() / v[2].w()) * w_reciprocal;
					if (z_interpolated < sample_depth_buf[sample_index]) {
						interpolated_color = (
							alpha * t.color[0] / v[0].w() +
							beta * t.color[1] / v[1].w() +
							gamma * t.color[2] / v[2].w()
							) * w_reciprocal;
						for (int i = 0; i < totalSamplePoint; i++) {
							sample_depth_buf[sample_index + i] = z_interpolated;
							sample_frame_buf[sample_index + i] = interpolated_color;
						}
					}
					continue;
				}

				bool pixel_color_set = false;
				for (int sx = 0; sx < sampleTimes; sx++) {
					for (int sy = 0; sy < sampleTimes; sy++) {
						float offset_x = (sx + 0.5f) / sampleTimes - 0.5f;
						float offset_y = (sy + 0.5f) / sampleTimes - 0.5f;
						float sample_x = static_cast<float>(x) + offset_x;
						float sample_y = static_cast<float>(y) + offset_y;
						if (insideTriangle(sample_x, sample_y, t.v)) {
							//鍥惧厓瑁呴厤鍜屽厜鏍呭寲 end
							//姣忎釜鐗囧厓涓€娆?鐗囨鐫€鑹?start
							auto barycentric = computeBarycentric2D(sample_x, sample_y, t.v);
							float alpha = std::get<0>(barycentric);
							float beta = std::get<1>(barycentric);
							float gamma = std::get<2>(barycentric);
							float w_reciprocal = 1.0f / (alpha / v[0].w() + beta / v[1].w() + gamma / v[2].w());
							float z_interpolated = (alpha * v[0].z() / v[0].w() +
								beta * v[1].z() / v[1].w() +
								gamma * v[2].z() / v[2].w()) * w_reciprocal;

							if (!pixel_color_set) {
								pixel_color_set = true;
								interpolated_color = (
									alpha * t.color[0] / v[0].w() +
									beta * t.color[1] / v[1].w() +
									gamma * t.color[2] / v[2].w()
									) * w_reciprocal;
							}
							//鐗囨鐫€鑹?end

							int sample_index = get_sample_index(x, y, sx, sy);

							if (z_interpolated < sample_depth_buf[sample_index]) {
								sample_depth_buf[sample_index] = z_interpolated;
								sample_frame_buf[sample_index] = interpolated_color;
								//涓轰粈涔堣瀛橀噰鏍风偣鐨刢olor鍛紵
								//澶勭悊澶氫釜涓夎褰㈣鐩栧悓涓€涓儚绱犵殑涓嶅悓閲囨牱鐐?
							}
							//杩欐槸 late-z锛屽湪鍍忕礌鐫€鑹蹭箣鍚庤繘琛屾繁搴︽祴璇?
						}
					}
				}
			}
		}
	}


	//TODO:杩欎釜鍑芥暟娓叉煋妯″瀷鏈夐棶棰樺晩锛侊紒锛?
	void rst::rasterizer::rasterize_triangle_MSAA(const Triangle& t, const std::array<Vector3f, 3>& view_pos) {
		auto v = t.toVector4();

		//鏍规嵁灞忓箷绌洪棿鍧愭爣璁＄畻浜岀淮鍖呭洿鐩?
		float minx = std::min({ v[0].x(), v[1].x(), v[2].x() });
		float maxx = std::max({ v[0].x(), v[1].x(), v[2].x() });
		float miny = std::min({ v[0].y(), v[1].y(), v[2].y() });
		float maxy = std::max({ v[0].y(), v[1].y(), v[2].y() });
		int screen_min_x = static_cast<int>(std::floor(minx));
		int screen_max_x = static_cast<int>(std::ceil(maxx));
		int screen_min_y = static_cast<int>(std::floor(miny));
		int screen_max_y = static_cast<int>(std::ceil(maxy));
		screen_min_x = std::max(0, screen_min_x);
		screen_max_x = std::min(width - 1, screen_max_x);
		screen_min_y = std::max(0, screen_min_y);
		screen_max_y = std::min(height - 1, screen_max_y);
		if (screen_min_x > screen_max_x || screen_min_y > screen_max_y) {
			return;
		}
		//濡傛灉杩欎釜涓夎褰㈡湁閮ㄥ垎鍦ㄥ睆骞曠┖闂翠互澶栵紝鍙互瑁佸壀杩欎釜涓夎褰?

		for (int x = screen_min_x; x <= screen_max_x; x++) {
			for (int y = screen_min_y; y <= screen_max_y; y++) {
				Vector3f interpolated_color;
				Vector3f pixel_color;
				auto pixelCoverage = checkPixelCoverage(x, y, t.v);
				if (pixelCoverage == PixelCoverage::FULLY_OUTSIDE) {
					continue;
				}
				if (pixelCoverage == PixelCoverage::FULLY_INSIDE) {
					int sample_index = get_sample_index(x, y, 0, 0);
					auto [alpha, beta, gamma] = computeBarycentric2D(x, y, t.v);
					float w_reciprocal = 1.0f / (alpha / v[0].w() + beta / v[1].w() + gamma / v[2].w());
					float z_interpolated = (alpha * v[0].z() / v[0].w() +
						beta * v[1].z() / v[1].w() +
						gamma * v[2].z() / v[2].w()) * w_reciprocal;

					if (z_interpolated < sample_depth_buf[sample_index]) {
						interpolated_color = interpolate(alpha, beta, gamma, t.color, t.getClipW(), w_reciprocal);
						//鐩存帴浠庨《鐐瑰睘鎬х殑棰滆壊灞炴€т腑鎻掑€?
						auto interpolated_normal = interpolate(alpha, beta, gamma, t.normal, t.getClipW(), w_reciprocal);
						auto interpolated_texcoords = interpolate(alpha, beta, gamma, t.tex_coords, t.getClipW(), w_reciprocal);
						auto interpolated_shadingcoords = interpolate(alpha, beta, gamma, view_pos, t.getClipW(), w_reciprocal);
						std::vector<Texture*> texture_ptrs;
						texture_ptrs.reserve(textures.size());
						for (auto& tex : textures)
							texture_ptrs.push_back(tex ? &(*tex) : nullptr);

						fragment_shader_payload payload(interpolated_color, interpolated_normal.normalized(),
							interpolated_texcoords, texture_ptrs);

						payload.view_pos = interpolated_shadingcoords;
						pixel_color = fragment_shader(payload);

						for (int i = 0; i < totalSamplePoint; i++) {
							sample_depth_buf[sample_index + i] = z_interpolated;
							sample_frame_buf[sample_index + i] = pixel_color;
						}
					}
					continue;
				}

				bool pixel_color_set = false;
				for (int sx = 0; sx < sampleTimes; sx++) {
					for (int sy = 0; sy < sampleTimes; sy++) {
						float offset_x = (sx + 0.5f) / sampleTimes - 0.5f;
						float offset_y = (sy + 0.5f) / sampleTimes - 0.5f;
						float sample_x = static_cast<float>(x) + offset_x;
						float sample_y = static_cast<float>(y) + offset_y;
						if (insideTriangle(sample_x, sample_y, t.v)) {
							//鍥惧厓瑁呴厤鍜屽厜鏍呭寲 end
							//姣忎釜鐗囧厓涓€娆?鐗囨鐫€鑹?start
							auto [alpha, beta, gamma] = computeBarycentric2D(sample_x, sample_y, t.v);
							float w_reciprocal = 1.0f / (alpha / v[0].w() + beta / v[1].w() + gamma / v[2].w());
							float z_interpolated = (alpha * v[0].z() / v[0].w() +
								beta * v[1].z() / v[1].w() +
								gamma * v[2].z() / v[2].w()) * w_reciprocal;

							if (!pixel_color_set) {
								pixel_color_set = true;
								interpolated_color = interpolate(alpha, beta, gamma, t.color, t.getClipW(), w_reciprocal);
								auto interpolated_normal = interpolate(alpha, beta, gamma, t.normal, t.getClipW(), w_reciprocal);
								auto interpolated_texcoords = interpolate(alpha, beta, gamma, t.tex_coords, t.getClipW(), w_reciprocal);
								auto interpolated_shadingcoords = interpolate(alpha, beta, gamma, view_pos, t.getClipW(), w_reciprocal);
								std::vector<Texture*> texture_ptrs;
								texture_ptrs.reserve(textures.size());
								for (auto& tex : textures)
									texture_ptrs.push_back(tex ? &(*tex) : nullptr);

								fragment_shader_payload payload(interpolated_color, interpolated_normal.normalized(),
									interpolated_texcoords, texture_ptrs);

								payload.view_pos = interpolated_shadingcoords;
								pixel_color = fragment_shader(payload);
							}
							//鐗囨鐫€鑹?end

							int sample_index = get_sample_index(x, y, sx, sy);

							if (z_interpolated < sample_depth_buf[sample_index]) {
								sample_depth_buf[sample_index] = z_interpolated;
								sample_frame_buf[sample_index] = pixel_color;
								//涓轰粈涔堣瀛橀噰鏍风偣鐨刢olor鍛紵
								//澶勭悊澶氫釜涓夎褰㈣鐩栧悓涓€涓儚绱犵殑涓嶅悓閲囨牱鐐?
							}
							//杩欐槸 late-z锛屽湪鍍忕礌鐫€鑹蹭箣鍚庤繘琛屾繁搴︽祴璇?
						}
					}
				}
			}
		}
	}

	//SSAA
	void rst::rasterizer::rasterize_triangle_SSAA(const Triangle& t) {
		auto v = t.toVector4();

		//鏍规嵁灞忓箷绌洪棿鍧愭爣璁＄畻浜岀淮鍖呭洿鐩?
		float minx = std::min({ v[0].x(), v[1].x(), v[2].x() });
		float maxx = std::max({ v[0].x(), v[1].x(), v[2].x() });
		float miny = std::min({ v[0].y(), v[1].y(), v[2].y() });
		float maxy = std::max({ v[0].y(), v[1].y(), v[2].y() });
		int screen_min_x = static_cast<int>(std::floor(minx));
		int screen_max_x = static_cast<int>(std::ceil(maxx));
		int screen_min_y = static_cast<int>(std::floor(miny));
		int screen_max_y = static_cast<int>(std::ceil(maxy));
		screen_min_x = std::max(0, screen_min_x);
		screen_max_x = std::min(width - 1, screen_max_x);
		screen_min_y = std::max(0, screen_min_y);
		screen_max_y = std::min(height - 1, screen_max_y);
		if (screen_min_x > screen_max_x || screen_min_y > screen_max_y) {
			return;
		}
		//濡傛灉杩欎釜涓夎褰㈡湁閮ㄥ垎鍦ㄥ睆骞曠┖闂翠互澶栵紝鍙互瑁佸壀杩欎釜涓夎褰?

		for (int x = screen_min_x; x <= screen_max_x; x++) {
			for (int y = screen_min_y; y <= screen_max_y; y++) {
				bool isFullyInside = false;
				auto pixelCoverage = checkPixelCoverage(x, y, t.v);
				if (pixelCoverage == PixelCoverage::FULLY_OUTSIDE) {
					continue;
				}
				if (pixelCoverage == PixelCoverage::FULLY_INSIDE) {
					isFullyInside = true;
				}

				for (int sx = 0; sx < sampleTimes; sx++) {
					for (int sy = 0; sy < sampleTimes; sy++) {
						float offset_x = (sx + 0.5f) / sampleTimes - 0.5f;
						float offset_y = (sy + 0.5f) / sampleTimes - 0.5f;
						float sample_x = static_cast<float>(x) + offset_x;
						float sample_y = static_cast<float>(y) + offset_y;
						if (isFullyInside || insideTriangle(sample_x, sample_y, t.v)) {
							auto barycentric = computeBarycentric2D(sample_x, sample_y, t.v);
							float alpha = std::get<0>(barycentric);
							float beta = std::get<1>(barycentric);
							float gamma = std::get<2>(barycentric);
							float w_reciprocal = 1.0f / (alpha / v[0].w() + beta / v[1].w() + gamma / v[2].w());
							float z_interpolated = (alpha * v[0].z() / v[0].w() +
								beta * v[1].z() / v[1].w() +
								gamma * v[2].z() / v[2].w()) * w_reciprocal;

							int sample_index = get_sample_index(x, y, sx, sy);

							if (z_interpolated < sample_depth_buf[sample_index]) {
								sample_depth_buf[sample_index] = z_interpolated;
								Vector3f interpolated_color = (
									alpha * t.color[0] / v[0].w() +
									beta * t.color[1] / v[1].w() +
									gamma * t.color[2] / v[2].w()
									) * w_reciprocal;
								sample_frame_buf[sample_index] = interpolated_color;
								//涓轰粈涔堣瀛橀噰鏍风偣鐨刢olor鍛紵
								//澶勭悊澶氫釜涓夎褰㈣鐩栧悓涓€涓儚绱犵殑涓嶅悓閲囨牱鐐?
							}
						}
					}
				}
			}
		}
	}

	bool rst::rasterizer::resolve_pixel_color(int x, int y, Vector3f& background, Vector3f& color) {
		Vector3f sum_color = { 0, 0, 0 };
		int valid_samples = 0;

		for (int sy = 0; sy < sampleTimes; sy++) {
			for (int sx = 0; sx < sampleTimes; sx++) {
				int idx = get_sample_index(x, y, sx, sy);

				// 妫€鏌ラ噰鏍风偣鏄惁鏈夋湁鏁堥鑹?
				if (sample_depth_buf[idx] < std::numeric_limits<float>::max()) {
					sum_color += sample_frame_buf[idx];
					valid_samples++;
				}
				else {
					// 閲囨牱鐐规湭琚鐩栵紝浣跨敤鑳屾櫙鑹?
					sum_color += background;
				}
			}
		}

		if (valid_samples > 0) {
			color = sum_color / static_cast<float>(sampleTimes * sampleTimes);
			return true;
		}
		return false;
	}

	void rst::rasterizer::resolve_frame() {
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				Vector3f color;
				Vector3f background = frame_buf[get_index(x, y)];
				if (resolve_pixel_color(x, y, background, color)) {
					set_pixel(Vector3f(x, y, 1.0f), color);
				}
			}
		}
	}

	void rst::rasterizer::set_vertex_shader(std::function<Eigen::Vector3f(vertex_shader_payload)> vert_shader)
	{
		vertex_shader = vert_shader;
	}

	void rst::rasterizer::set_fragment_shader(std::function<Eigen::Vector3f(fragment_shader_payload)> frag_shader)
	{
		fragment_shader = frag_shader;
	}

}


