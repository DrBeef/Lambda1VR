
package com.drbeef.lambda1vr;


import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;

import android.os.Bundle;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.WindowManager;
import android.view.KeyEvent;

import static android.system.Os.setenv;


@SuppressLint("SdCardPath") public class GLES3JNIActivity extends Activity implements SurfaceHolder.Callback
{
	// Load the gles3jni library right away to make sure JNI_OnLoad() gets called as the very first thing.
	static
	{
		System.loadLibrary( "xash" );
	}

	private static final String TAG = "Lambda1VR";

	private int permissionCount = 0;
	private static final int READ_EXTERNAL_STORAGE_PERMISSION_ID = 1;
	private static final int WRITE_EXTERNAL_STORAGE_PERMISSION_ID = 2;

	String commandLineParams;

	private SurfaceView mView;
	private SurfaceHolder mSurfaceHolder;
	private long mNativeHandle;
	
	@Override protected void onCreate( Bundle icicle )
	{
		Log.v( TAG, "----------------------------------------------------------------" );
		Log.v( TAG, "GLES3JNIActivity::onCreate()" );
		super.onCreate( icicle );

		mView = new SurfaceView( this );
		setContentView( mView );
		mView.getHolder().addCallback( this );

		// Force the screen to stay on, rather than letting it dim and shut off
		// while the user is watching a movie.
		getWindow().addFlags( WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON );

		// Force screen brightness to stay at maximum
		WindowManager.LayoutParams params = getWindow().getAttributes();
		params.screenBrightness = 1.0f;
		getWindow().setAttributes( params );



		checkPermissionsAndInitialize();
	}

	/** Initializes the Activity only if the permission has been granted. */
	private void checkPermissionsAndInitialize() {
		// Boilerplate for checking runtime permissions in Android.
		if (ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE)
				!= PackageManager.PERMISSION_GRANTED){
			ActivityCompat.requestPermissions(
					GLES3JNIActivity.this,
					new String[] {Manifest.permission.WRITE_EXTERNAL_STORAGE},
					WRITE_EXTERNAL_STORAGE_PERMISSION_ID);
		}
		else
		{
			permissionCount++;
		}

		if (ContextCompat.checkSelfPermission(this, Manifest.permission.READ_EXTERNAL_STORAGE)
				!= PackageManager.PERMISSION_GRANTED)
		{
			ActivityCompat.requestPermissions(
					GLES3JNIActivity.this,
					new String[] {Manifest.permission.READ_EXTERNAL_STORAGE},
					READ_EXTERNAL_STORAGE_PERMISSION_ID);
		}
		else
		{
			permissionCount++;
		}

		if (permissionCount == 2) {
			// Permissions have already been granted.
			create();
		}
	}

	/** Handles the user accepting the permission. */
	@Override
	public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] results) {
		if (requestCode == READ_EXTERNAL_STORAGE_PERMISSION_ID) {
			if (results.length > 0 && results[0] == PackageManager.PERMISSION_GRANTED) {
				permissionCount++;
			}
			else
			{
				System.exit(0);
			}
		}

		if (requestCode == WRITE_EXTERNAL_STORAGE_PERMISSION_ID) {
			if (results.length > 0 && results[0] == PackageManager.PERMISSION_GRANTED) {
				permissionCount++;
			}
			else
			{
				System.exit(0);
			}
		}

		checkPermissionsAndInitialize();
	}

	public void create()
	{
		copy_asset(getFilesDir().getPath(), "extras.pak", false);
		copy_asset("/sdcard/xash/", "commandline.txt", false); // Copy in case user has deleted their config


		//Read these from a file and pass through
		commandLineParams = new String("xash3d -dev 3 -log");

		//See if user is trying to use command line params
		if(new File("/sdcard/xash/commandline.txt").exists()) // should exist!
		{
			BufferedReader br;
			try {
				br = new BufferedReader(new FileReader("/sdcard/xash/commandline.txt"));
				String s;
				StringBuilder sb=new StringBuilder(0);
				while ((s=br.readLine())!=null)
					sb.append(s + " ");
				br.close();

				commandLineParams = new String(sb.toString());
			} catch (FileNotFoundException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			} catch (IOException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
		}

		String[] params = commandLineParams.split(" ");
		
		String game = "valve";
		
		int i = 0;
		for (i = 0; i < params.length; ++i)
		{
			if (params[i].compareTo("-game") == 0)
			{
				game = params[i+1];
			}
		}

		//game configuration
		copy_asset("/sdcard/xash/" + game + "/", "config.cfg", false); // Copy in case user has deleted their config

		//special commands
		copy_asset("/sdcard/xash/" + game + "/", "commands.lst", false); // Copy in case user has deleted their config

		//Copy our special stuff
		copy_asset("/sdcard/xash/" + game + "/", "sprites/s_stealth.spr", true);
		copy_asset("/sdcard/xash/" + game + "/", "sprites/vignette.tga", true);
		copy_asset("/sdcard/xash/valve/", "sprites/vignette.tga", true); //seems to need to be here for some people

		//Copy modified weapon models - This is the base set
		if (!(new File("/sdcard/xash/" + game + "/models/no_copy").exists()))
		{
			//Colt (A-16)
			copy_asset("/sdcard/xash/" + game + "/", "models/v_9mmar.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "models/p_9mmar.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "models/w_9mmar.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "sound/items/clipinsert1.wav", true);
			copy_asset("/sdcard/xash/" + game + "/", "sound/items/cliprelease1.wav", true);
			copy_asset("/sdcard/xash/" + game + "/", "sound/weapons/t_m4_boltpull.wav", true);
			copy_asset("/sdcard/xash/" + game + "/", "sound/weapons/t_m4_deploy.wav", true);
			copy_asset("/sdcard/xash/" + game + "/", "sound/weapons/t_m4_m203_in.wav", true);
			copy_asset("/sdcard/xash/" + game + "/", "sound/weapons/t_m4_m203_out.wav", true);
			copy_asset("/sdcard/xash/" + game + "/", "sound/weapons/t_m4_m203_shell.wav", true);

			//Colt Pistol M1911
			copy_asset("/sdcard/xash/" + game + "/", "models/v_9mmhandgun.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "models/p_9mmhandgun.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "models/w_9mmhandgun.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "sound/weapons/glock_magin.wav", true);
			copy_asset("/sdcard/xash/" + game + "/", "sound/weapons/glock_magout.wav", true);
			copy_asset("/sdcard/xash/" + game + "/", "sound/weapons/glock_slideforward.wav", true);

			//357 Python
			copy_asset("/sdcard/xash/" + game + "/", "models/v_357.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "models/p_357.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "models/w_357.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "sound/weapons/357_shot1.wav", true);
			copy_asset("/sdcard/xash/" + game + "/", "sound/weapons/357_shot2.wav", true);

			//RGD Grenade
			copy_asset("/sdcard/xash/" + game + "/", "models/v_grenade.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "models/w_grenade.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "models/p_grenade.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "sound/weapons/grenade_pinpull.wav", true);
			copy_asset("/sdcard/xash/" + game + "/", "sound/weapons/grenade_throw.wav", true);
			copy_asset("/sdcard/xash/" + game + "/", "sound/weapons/grenade_draw.wav", true);
			copy_asset("/sdcard/xash/" + game + "/", "sound/weapons/explode3.wav", true);
			copy_asset("/sdcard/xash/" + game + "/", "sound/weapons/explode4.wav", true);
			copy_asset("/sdcard/xash/" + game + "/", "sound/weapons/explode5.wav", true);

			//Shotgun Benelli M3
			copy_asset("/sdcard/xash/" + game + "/", "models/v_shotgun.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "models/p_shotgun.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "models/w_shotgun.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "sound/weapons/m3_insertshell.wav", true);
			copy_asset("/sdcard/xash/" + game + "/", "sound/weapons/m3_pump.wav", true);

			//Stalker Rocket
			copy_asset("/sdcard/xash/" + game + "/", "models/v_rpg.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "models/p_rpg.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "models/w_rpg.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "models/rpgrocket.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "models/v_rpgammo.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "sound/weapons/rocket1.wav", true);
			copy_asset("/sdcard/xash/" + game + "/", "sound/weapons/rocketfire1.wav", true);
			copy_asset("/sdcard/xash/" + game + "/", "sound/weapons/rpg7_reload.wav", true);

			//Satchel
			copy_asset("/sdcard/xash/" + game + "/", "models/v_satchel.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "models/v_satchel_radio.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "models/w_satchel.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "models/p_satchel.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "models/p_satchel_radio.mdl", true);

			//Tripmine
			copy_asset("/sdcard/xash/" + game + "/", "models/v_tripmine.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "models/p_tripmine.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "sound/weapons/mine_activate.wav", true);
			copy_asset("/sdcard/xash/" + game + "/", "sound/weapons/mine_deploy.wav", true);
			copy_asset("/sdcard/xash/" + game + "/", "sound/weapons/tripmine_ant.wav", true);
			copy_asset("/sdcard/xash/" + game + "/", "sound/weapons/tripmine_button.wav", true);
			copy_asset("/sdcard/xash/" + game + "/", "sound/weapons/tripmine_move.wav", true);

			//BMS Crossbow
			copy_asset("/sdcard/xash/" + game + "/", "models/v_crossbow.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "models/p_crossbow.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "models/w_crossbow.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "models/w_crossbow_clip.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "sound/weapons/natianul.wav", true);
			copy_asset("/sdcard/xash/" + game + "/", "sound/weapons/xbow.wav", true);
			copy_asset("/sdcard/xash/" + game + "/", "sound/weapons/xbow_fire1.wav", true);
			copy_asset("/sdcard/xash/" + game + "/", "sound/weapons/xbow_reload1.wav", true);


			//Egon (Overhaul)
			copy_asset("/sdcard/xash/" + game + "/", "models/v_egon.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "models/p_egon.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "models/w_egon.mdl", true);

			//Gauss (Overhaul)
			copy_asset("/sdcard/xash/" + game + "/", "models/v_gauss.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "models/p_gauss.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "models/w_gauss.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "models/w_gaussammo.mdl", true);

			//HGun (Overhaul)
			copy_asset("/sdcard/xash/" + game + "/", "models/v_hgun.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "models/p_hgun.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "models/w_hgun.mdl", true);

			//Squeak (Overhaul)
			copy_asset("/sdcard/xash/" + game + "/", "models/v_squeak.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "models/p_squeak.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "models/w_squeak.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "models/w_sqknest.mdl", true);

			//Crowbar
			copy_asset("/sdcard/xash/" + game + "/", "models/v_crowbar.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "models/w_crowbar.mdl", true);
			copy_asset("/sdcard/xash/" + game + "/", "sound/weapons/cbar_draw.wav", true);
			copy_asset("/sdcard/xash/" + game + "/", "sound/weapons/cbar_hit1.wav", true);
			copy_asset("/sdcard/xash/" + game + "/", "sound/weapons/cbar_hit2.wav", true);
			copy_asset("/sdcard/xash/" + game + "/", "sound/weapons/cbar_hitbod1.wav", true);
			copy_asset("/sdcard/xash/" + game + "/", "sound/weapons/cbar_hitbod2.wav", true);
			copy_asset("/sdcard/xash/" + game + "/", "sound/weapons/cbar_hitbod3.wav", true);
			copy_asset("/sdcard/xash/" + game + "/", "sound/weapons/cbar_miss1.wav", true);

			copy_asset("/sdcard/xash/" + game + "/", "models/v_torch.mdl", true);
		}

		//Copy Opposing Force specific models
		if (game.equalsIgnoreCase("gearbox") &&
				!(new File("/sdcard/xash/" + game + "/models/no_copy").exists()))
		{
			//Scope vignette texture
			copy_asset("/sdcard/xash/" + game + "/", "sprites/scope.tga", true);
			copy_asset("/sdcard/xash/valve/", "sprites/scope.tga", true);

			//Sniper Rifle
			copy_asset("/sdcard/xash/", "gearbox/models/v_m40a1.mdl", true);
			copy_asset("/sdcard/xash/", "gearbox/models/w_m40a1.mdl", true);
			copy_asset("/sdcard/xash/", "gearbox/sound/weapons/scout_clipin.wav", true);
			copy_asset("/sdcard/xash/", "gearbox/sound/weapons/scout_clipout.wav", true);
			copy_asset("/sdcard/xash/", "gearbox/sound/weapons/sniper_fire.wav", true);

			//Pipe Wrench
			copy_asset("/sdcard/xash/", "gearbox/models/v_pipe_wrench.mdl", true);
			copy_asset("/sdcard/xash/", "gearbox/models/w_pipe_wrench.mdl", true);
			copy_asset("/sdcard/xash/", "gearbox/models/p_pipe_wrench.mdl", true);

			//Penguins
			new File("/sdcard/xash/gearbox/sound/penguin/").mkdirs(); // Make the penguin directory as it won't exist
			copy_asset("/sdcard/xash/", "gearbox/sound/penguin/penguin_die1.wav", true);
			copy_asset("/sdcard/xash/", "gearbox/sound/penguin/penguin_deploy1.wav", true);
			copy_asset("/sdcard/xash/", "gearbox/sound/penguin/penguin_hunt1.wav", true);
			copy_asset("/sdcard/xash/", "gearbox/sound/penguin/penguin_hunt2.wav", true);
			copy_asset("/sdcard/xash/", "gearbox/sound/penguin/penguin_hunt3.wav", true);

			//Knife
			copy_asset("/sdcard/xash/", "gearbox/models/v_knife.mdl", true);
			copy_asset("/sdcard/xash/", "gearbox/models/w_knife.mdl", true);
			copy_asset("/sdcard/xash/", "gearbox/models/p_knife.mdl", true);
		}

		//Set default environment
		try {
			setenv("XASH3D_BASEDIR", "/sdcard/xash/", true);
			setenv("XASH3D_ENGLIBDIR", getFilesDir().getParentFile().getPath() + "/lib", true);
			setenv("XASH3D_GAMELIBDIR", getFilesDir().getParentFile().getPath() + "/lib", true);
			setenv("XASH3D_GAMEDIR", "valve", true);
			setenv( "XASH3D_EXTRAS_PAK1", getFilesDir().getPath() + "/extras.pak", true );

			//If game is gearbox (Opposing Force) set the library file suffix to opfor
			if (game.equalsIgnoreCase("gearbox"))
			{
				setenv("XASH3D_LIBSUFFIX", "_opfor", true);

				//Use pipewrench as backpack weapon in opposing force
				setenv("VR_BACKPACK_WEAPON", "weapon_pipewrench", true);
			}
			else
			{
				setenv("XASH3D_LIBSUFFIX", "", true);
			}
		}
		catch (Exception e)
		{

		}
		
		mNativeHandle = GLES3JNILib.onCreate( this, commandLineParams );
	}
	
	public void copy_asset(String path, String name, boolean forceOverwrite) {
		File f = new File(path + "/" + name);
		if (!f.exists() || forceOverwrite) {
			
			//Ensure we have an appropriate folder
			new File(path).mkdirs();
			_copy_asset(name, path + "/" + name);
		}
	}

	public void _copy_asset(String name_in, String name_out) {
		AssetManager assets = this.getAssets();

		try {
			InputStream in = assets.open(name_in);
			OutputStream out = new FileOutputStream(name_out);

			copy_stream(in, out);

			out.close();
			in.close();

		} catch (Exception e) {

			e.printStackTrace();
		}

	}

	public static void copy_stream(InputStream in, OutputStream out)
			throws IOException {
		byte[] buf = new byte[1024];
		while (true) {
			int count = in.read(buf);
			if (count <= 0)
				break;
			out.write(buf, 0, count);
		}
	}


	@Override protected void onStart()
	{
		Log.v( TAG, "GLES3JNIActivity::onStart()" );
		super.onStart();

		GLES3JNILib.onStart( mNativeHandle );
	}

	@Override protected void onResume()
	{
		Log.v( TAG, "GLES3JNIActivity::onResume()" );
		super.onResume();

		GLES3JNILib.onResume( mNativeHandle );
	}

	@Override protected void onPause()
	{
		Log.v( TAG, "GLES3JNIActivity::onPause()" );
		GLES3JNILib.onPause( mNativeHandle );
		super.onPause();
	}

	@Override protected void onStop()
	{
		Log.v( TAG, "GLES3JNIActivity::onStop()" );
		GLES3JNILib.onStop( mNativeHandle );
		super.onStop();
	}

	@Override protected void onDestroy()
	{
		Log.v( TAG, "GLES3JNIActivity::onDestroy()" );

		if ( mSurfaceHolder != null )
		{
			GLES3JNILib.onSurfaceDestroyed( mNativeHandle );
		}

		GLES3JNILib.onDestroy( mNativeHandle );

		super.onDestroy();
		mNativeHandle = 0;
	}

	@Override public void surfaceCreated( SurfaceHolder holder )
	{
		Log.v( TAG, "GLES3JNIActivity::surfaceCreated()" );
		if ( mNativeHandle != 0 )
		{
			GLES3JNILib.onSurfaceCreated( mNativeHandle, holder.getSurface() );
			mSurfaceHolder = holder;
		}
	}

	@Override public void surfaceChanged( SurfaceHolder holder, int format, int width, int height )
	{
		Log.v( TAG, "GLES3JNIActivity::surfaceChanged()" );
		if ( mNativeHandle != 0 )
		{
			GLES3JNILib.onSurfaceChanged( mNativeHandle, holder.getSurface() );
			mSurfaceHolder = holder;
		}
	}
	
	@Override public void surfaceDestroyed( SurfaceHolder holder )
	{
		Log.v( TAG, "GLES3JNIActivity::surfaceDestroyed()" );
		if ( mNativeHandle != 0 )
		{
			GLES3JNILib.onSurfaceDestroyed( mNativeHandle );
			mSurfaceHolder = null;
		}
	}
}
